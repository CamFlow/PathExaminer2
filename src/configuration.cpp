#include <algorithm>
#include <iostream>

#include <gcc-plugin.h>
#include <gimple.h>
#include <tree.h>
#include <function.h>
#include <tree-flow.h>
#include <tree-flow-inline.h>

#include <memory>

#include "configuration.h"
#include "constraint.h"


Configuration& Configuration::operator<<(gimple stmt)
{
	switch (gimple_code(stmt)) {
		case GIMPLE_ASSIGN:
			doGimpleAssign(stmt);
			break;
		case GIMPLE_PHI:
			doGimplePhi(stmt);
			break;
		case GIMPLE_CALL:
			doGimpleCall(stmt);
			break;
		case GIMPLE_ASM:
			resetAllVarMem();
			break;
		default:
			; //nothing to do
	}
	return *this;
}

Configuration& Configuration::operator<<(const Constraint& c)
{
	ValueRange& v = getValueRangeForVar(c.lhs);
	switch (c.rel) {
		case EQ_EXPR:
		case NE_EXPR:
		case LT_EXPR:
		case LE_EXPR:
		case GT_EXPR:
		case GE_EXPR:
			v.addConstraint(c);
			break;
		default:
			; //we should probably raise an exception here
			  //it means that we don't handle one of the operator
			  //or that we mis-parsed the constraint
	}
	return *this;
}

void Configuration::doGimpleCall(gimple stmt)
{
	tree lhs = gimple_call_lhs(stmt);
	if (lhs && lhs != NULL_TREE)
		getValueRangeForVar(lhs)._constraints.clear();
	resetAllVarMem();
}

void Configuration::doGimpleAssign(gimple stmt)
{
	if (gimple_num_ops(stmt) > 2)
		return; //we cannot do nothing if stmt is
			//more than a simple assignment
	tree lhs = gimple_assign_lhs(stmt);
	tree rhs = gimple_assign_rhs1(stmt);

	// TODO the following is not correct, we have to erase all constraints
	// concerning lhs, not only those with lhs on the left-hand sides of
	// constraints
	if (!is_gimple_reg(lhs)) {
		resetVar(lhs);
	}
	ValueRange& val = getValueRangeForVar(lhs);
	val.addConstraint(Constraint(lhs,EQ_EXPR,rhs));
}

void Configuration::doGimplePhi(gimple stmt)
{
	if (gimple_num_ops(stmt) > 2)
		return; //we cannot do nothing if stmt is
			//more than a simple assignment
	tree lhs = gimple_phi_result(stmt);
	tree rhs = gimple_phi_arg_def(stmt, _indexLastEdgeTaken);


	// TODO the following is not correct, we have to erase all constraints
	// concerning lhs, not only those with lhs on the left-hand sides of
	// constraints
	if (!is_gimple_reg(lhs)) {
		resetVar(lhs);
	}
	ValueRange& val = getValueRangeForVar(lhs);
	val.addConstraint(Constraint(lhs,EQ_EXPR,rhs));
}

void ValueRange::addConstraint(const Constraint& c)
{
	const std::map<tree_code, term_t(*)(term_t,term_t)> ops{
		{GT_EXPR,  yices_arith_gt_atom},
			{LT_EXPR,  yices_arith_lt_atom},
			{GE_EXPR,  yices_arith_geq_atom},
			{LE_EXPR,  yices_arith_leq_atom},
			{EQ_EXPR,  yices_arith_eq_atom},
			{NE_EXPR,  yices_arith_neq_atom}
	};

	if (is_gimple_variable(c.rhs))
		_terms.push_back(
				ops.at(c.rel)(
					yices_get_term_by_name(Configuration::strForTree(c.lhs).c_str()),
					yices_get_term_by_name(Configuration::strForTree(c.rhs).c_str()))
				);
	else if (TREE_CODE(c.rhs) == INTEGER_CST)
		_terms.push_back(
				ops.at(c.rel)(
					yices_get_term_by_name(Configuration::strForTree(c.lhs).c_str()),
					yices_int64(TREE_INT_CST_HIGH(c.rhs) << 32) + TREE_INT_CST_LOW(c.rhs))
				);
	else
		return; // otherwise we don't know what lhs is compared to
	_constraints.push_back(c);
}

ValueRange::operator bool()
{
	return checkVectorOfConstraints(_terms);

}

bool ValueRange::checkVectorOfConstraints(std::vector<term_t>& terms)
{
	term_t conjunct = yices_and(terms.size(), terms.data());
	auto context_deleter = [](context_t* c) { yices_free_context(c); };
	std::unique_ptr<context_t,decltype(context_deleter)&> ctx{yices_new_context(nullptr), context_deleter};
	int code = yices_assert_formula(ctx.get(), conjunct);
	if (code < 0) {
		yices_print_error(stderr);
		throw std::runtime_error("Assert failed on formula");
	}
	return yices_check_context(ctx.get(), nullptr) & (STATUS_SAT | STATUS_UNKNOWN);
}

Configuration::operator bool() {
	std::vector<term_t> all_constraints;
	for (auto&& p : _varsMem)
		for (term_t& t : p.second._terms)
			all_constraints.emplace_back(t);
	for (auto&& p : _varsTemp)
		for (term_t& t : p.second._terms)
			all_constraints.emplace_back(t);
	return ValueRange::checkVectorOfConstraints(all_constraints);
}

ValueRange& Configuration::getValueRangeForVar(tree var) {
	std::map<tree,ValueRange>& map = is_gimple_reg(var) ? _varsMem : _varsTemp;

	auto it = map.find(var);
	if (it == map.end()) {
		term_t res = yices_new_uninterpreted_term(Yices_int);
		std::string name;
		yices_set_term_name(res, name.c_str());
		it = map.emplace(var,ValueRange()).first;
	}

	return it->second;
}

void Configuration::resetVar(tree var) {
	auto it = _varsMem.find(var);
	if (it != _varsMem.end())
		it->second._constraints.clear();

	it = _varsTemp.find(var);
	if (it != _varsTemp.end())
		it->second._constraints.clear();

	// erase all constraints about var everywhere

}

void Configuration::resetAllVarMem()
{
	for (auto it = _ptrDestination.begin() ; it != _ptrDestination.end() ; )
	{
		const tree ptr = it->first;
		if(_varsMem.find(ptr) != _varsMem.end())
			_ptrDestination.erase(it++);
		else
			it++;
	}

	for (auto& p : _varsMem)
		p.second._constraints.clear();
}

void Configuration::addConstraint(const Constraint& c) {
	ValueRange& r = getValueRangeForVar(c.lhs);
	r.addConstraint(c);

	//Special case : int* p; int v; p = &v;
	//we want to remember that *p is aliased to v
	if (POINTER_TYPE_P(TREE_TYPE(c.lhs)) &&
	    c.rel == EQ_EXPR) {
		_ptrDestination[c.lhs] = c.rhs;
	}
}

const std::string& Configuration::strForTree(tree t)
{
	auto it = _strings.find(t);
	if (it != _strings.end())
		return it->second;

	static unsigned int ssaCounter = 0;
	static unsigned int varCounter = 0;

	std::string res;
	if (TREE_CODE(t) == SSA_NAME) {
		tree name = SSA_NAME_IDENTIFIER(t);
		if (!name || name == NULL_TREE)
			res = "<ssa " + std::to_string(ssaCounter++) + ">";
		else
			res = IDENTIFIER_POINTER(name);
		res += "." + std::to_string(SSA_NAME_VERSION(t));
	} else if (TREE_CODE(t) == VAR_DECL) {
		tree name = DECL_NAME(t);
		if (!name || name == NULL_TREE)
			res = "<var " + std::to_string(varCounter++) + ">";
		else
			res = IDENTIFIER_POINTER(name);
	} else {
		std::cerr << "Warning: trying to get a name for " << tree_code_name[TREE_CODE(t)] << std::endl;
	}

	return _strings.emplace(t,std::move(res)).first->second;
}
