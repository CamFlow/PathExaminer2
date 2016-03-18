#include <algorithm>
#include <iostream>

#include <gcc-plugin.h>
#include <gimple.h>
#include <tree.h>
#include <function.h>
#include <tree-flow.h>
#include <tree-flow-inline.h>
#include <tree-ssa-alias.h>

#include <memory>

#include "configuration.h"
#include "constraint.h"

Configuration::Configuration() :
	_indexLastEdgeTaken{0},
	_lastBB{ENTRY_BLOCK_PTR}
{
	compute_may_aliases(); //needed for the points-to oracle
}

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
	switch (c.rel) {
		case EQ_EXPR:
		case NE_EXPR:
		case LT_EXPR:
		case LE_EXPR:
		case GT_EXPR:
		case GE_EXPR:
			tryAddConstraint(c);
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
		resetVar(lhs);

	resetAllVarMem();
}

void Configuration::doGimpleAssign(gimple stmt)
{
	if (!gimple_assign_copy_p(stmt))
		return; //we can do nothing if stmt is
			//more than a simple assignment
	tree lhs = gimple_assign_lhs(stmt);
	tree rhs = gimple_assign_rhs1(stmt);

	if (INDIRECT_REF_P(lhs)) { //this is a mem node
		tree pointer = TREE_OPERAND(lhs, 0);
		auto it = _ptrDestination.find(pointer);
		if (it != _ptrDestination.end()) {
			resetVar(it->second);
			tryAddConstraint(Constraint(it->second,EQ_EXPR,rhs));
		} else {
			pt_solution& ptSol = get_ptr_info(pointer)->pt;
			auto mayDeref = [&ptSol,&pointer](tree var) -> bool {
				if (ptSol.escaped || ptSol.anything)
					return alias_sets_conflict_p(get_alias_set(var),get_deref_alias_set(pointer));
				else
					return bitmap_bit_p(ptSol.vars, DECL_PT_UID(var));
			};

			unsigned int ix;
			tree var;
			FOR_EACH_LOCAL_DECL(cfun, ix, var) {
				if (mayDeref(var)) {
					resetVar(var);
				}
			}
		}
	} else {
		resetVar(lhs);
		tryAddConstraint(Constraint(lhs,EQ_EXPR,rhs));
	}
}

void Configuration::doGimplePhi(gimple stmt)
{
	tree lhs = gimple_phi_result(stmt);
	tree rhs = gimple_phi_arg_def(stmt, _indexLastEdgeTaken);


	resetVar(lhs);
	tryAddConstraint(Constraint(lhs,EQ_EXPR,rhs));
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
	term_t t;

	if (is_gimple_variable(c.rhs))
		t = ops.at(c.rel)(
			yices_get_term_by_name(Configuration::strForTree(c.lhs).c_str()),
			yices_get_term_by_name(Configuration::strForTree(c.rhs).c_str()));
	else if (TREE_CODE(c.rhs) == INTEGER_CST)
		t = ops.at(c.rel)(
			yices_get_term_by_name(Configuration::strForTree(c.lhs).c_str()),
			yices_int64(TREE_INT_CST_HIGH(c.rhs) << 32) + TREE_INT_CST_LOW(c.rhs));
	else
		return; // otherwise we don't know what lhs is compared to

	_constraints.emplace(c,t);
}

ValueRange::operator bool()
{
	std::vector<term_t> terms;
	std::transform(_constraints.begin(), _constraints.end(),
			std::back_inserter(terms),
			[](const std::pair<Constraint,term_t>& p) { return p.second; } );
	return checkVectorOfConstraints(terms);
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
	return bool(_allConstraints);
}

void Configuration::resetVar(tree var) {
	// erase all constraints about var everywhere
	std::map<Constraint,term_t>& constraints = _allConstraints._constraints;
	for (auto it = constraints.begin() ; it != constraints.end() ;)
	{
		const Constraint& c = it->first;
		if (c.lhs == var || c.rhs == var)
			it = constraints.erase(it);
		else
			++it;
	}

	//if var is a pointer, we lose the information about its value
	_ptrDestination.erase(var);
}

void Configuration::resetAllVarMem()
{
	std::map<Constraint,term_t>& constraints = _allConstraints._constraints;
	for (auto it = constraints.begin() ; it != constraints.end() ;)
	{
		const Constraint& c = it->first;
		if (is_gimple_reg(c.lhs) && is_gimple_reg(c.rhs))
			++it;
		else
			it = constraints.erase(it);
	}

	for (auto it = _ptrDestination.begin() ; it != _ptrDestination.end() ;)
	{
		const tree pointer = it->first;
		if (is_gimple_reg(pointer))
			++it;
		else
			it = _ptrDestination.erase(it);
	}
}

bool Configuration::tryAddConstraint(const Constraint& c)
{
	if (!SSA_VAR_P(c.lhs) && !SSA_VAR_P(c.rhs))
		return false;

	if (is_global_var(c.lhs) || is_global_var(c.rhs))
		return false;

	if ((DECL_P(c.lhs) && TREE_THIS_VOLATILE(c.lhs)) ||
	    (DECL_P(c.rhs) && TREE_THIS_VOLATILE(c.rhs)))
		return false;

	_allConstraints.addConstraint(c);

	//Special case : int* p; int v; p = &v;
	//we want to remember that *p is aliased to v
	if (POINTER_TYPE_P(TREE_TYPE(c.lhs)) &&
	    c.rel == EQ_EXPR) {
		_ptrDestination[c.lhs] = c.rhs;
	}

	return true;
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
