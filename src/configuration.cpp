#include <algorithm>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>
#include <map>
#include <functional>

#include <cassert>
#include <cstring>
#include <gcc-plugin.h>
#include <gimple.h>
#include <tree.h>
#include <function.h>
#include <tree-flow.h>
#include <tree-flow-inline.h>
#include <tree-ssa-alias.h>

#include "configuration.h"
#include "constraint.h"
#include "rich_basic_block.h"
#include "debug.h"

const type_t Configuration::YICES_INT{yices_int_type()};

std::map<tree,std::string> Configuration::_strings;

Configuration::Configuration() :
	_indexLastEdgeTaken{0}
{
	debug() << "Configuration created, _constraints size: " << _constraints.size() << std::endl;
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
	if (!gimple_assign_single_p(stmt)) {
		debug() << "the statement has several rhs args" << std::endl;
		return; //we can do nothing if stmt is
			//more than a simple copy (e.g. if it's an operation)
	}

	tree lhs = gimple_assign_lhs(stmt);
	tree rhs = gimple_assign_rhs1(stmt);

	if (INDIRECT_REF_P(lhs) || TREE_CODE(lhs) == MEM_REF
		|| TREE_CODE(lhs) == TARGET_MEM_REF) { //this is a mem node
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
	} else if (is_gimple_variable(lhs)) {
		debug() << strForTree(lhs) << " is a variable" << std::endl;
		resetVar(lhs);
		if (!gimple_clobber_p(stmt))
			tryAddConstraint(Constraint(lhs,EQ_EXPR,rhs));
	} else {
		throw std::runtime_error(std::string("Unhandled assignment: ") + tree_code_name[TREE_CODE(lhs)]);
	}
}

void Configuration::doGimplePhi(gimple stmt)
{
	tree lhs = gimple_phi_result(stmt);
	tree rhs = gimple_phi_arg_def(stmt, _indexLastEdgeTaken);

	if (is_gimple_reg(lhs)) //dont care about .MEM phi nodes
		tryAddConstraint(Constraint(lhs,EQ_EXPR,rhs));
}

void Configuration::doAddConstraint(Constraint c)
{
	static const std::map<tree_code, term_t(*)(term_t,term_t)> ops{
		{GT_EXPR,  yices_arith_gt_atom},
		{LT_EXPR,  yices_arith_lt_atom},
		{GE_EXPR,  yices_arith_geq_atom},
		{LE_EXPR,  yices_arith_leq_atom},
		{EQ_EXPR,  yices_arith_eq_atom},
		{NE_EXPR,  yices_arith_neq_atom}
	};
	term_t t;

	t = ops.at(c.rel)(
		Configuration::getNormalizedTerm(c.lhs),
		Configuration::getNormalizedTerm(c.rhs)
	);
	debug() << "After normalization, new constraint: " << std::endl;
	yices_pp_term(stderr, t, 40, 1, 0);

	debug() << "Constraint about to be inserted, size: " << _constraints.size() << std::endl;
	for (const auto& p : _constraints) {
		debug() << "\t";
		yices_pp_term(stderr, p.second, 40, 1, 0);
	}
	_constraints.emplace_back(std::move(c),t);
	debug() << "Constraint inserted, size: " << _constraints.size() << std::endl;
}

Configuration::operator bool()
{
	std::vector<term_t> terms;
	debug() << "Building the set of constraints" << std::endl;
	std::transform(_constraints.cbegin(), _constraints.cend(),
			std::back_inserter(terms),
			[](const std::pair<Constraint,term_t>& p) {
				return p.second;
			}
	);
	debug() << "Set of constraints built" << std::endl;
	return checkVectorOfConstraints(terms);
}

bool Configuration::checkVectorOfConstraints(std::vector<term_t>& terms)
{
	term_t conjunct = yices_and(terms.size(), terms.data());
	yices_pp_term(stderr, conjunct, 120, 50, 0);
	auto context_deleter = [](context_t* c) { yices_free_context(c); };
	std::unique_ptr<context_t,decltype(context_deleter)&> ctx{yices_new_context(nullptr), context_deleter};
	int code = yices_assert_formula(ctx.get(), conjunct);
	if (code < 0) {
		yices_print_error(stderr);
		throw std::runtime_error("Assert failed on formula");
	}
	bool res = yices_check_context(ctx.get(), nullptr) & (STATUS_SAT | STATUS_UNKNOWN);

	if (res)
		debug() << "Yices says satisfiable" << std::endl;
	else
		debug() << "Yices says unsatisfiable" << std::endl;
	return res;
}

void Configuration::resetVar(tree var) {
	// erase all constraints about var everywhere
	_constraints.erase(
		std::remove_if(_constraints.begin(), _constraints.end(),
			[&var](const std::pair<Constraint,term_t>& p) {
				const Constraint& c = p.first;
				return (c.lhs == var || c.rhs == var);
			}),
		_constraints.end()
	);

	//if var is a pointer, we lose the information about its value
	_ptrDestination.erase(var);
}

void Configuration::resetAllVarMem()
{
	_constraints.erase(
		std::remove_if(_constraints.begin(), _constraints.end(),
			[](const std::pair<Constraint,term_t>& p) {
				const Constraint& c = p.first;
				return (!is_gimple_reg(c.lhs) ||
				        (is_gimple_variable(c.rhs) && !is_gimple_reg(c.rhs)));
			}),
		_constraints.end()
	);


	for (auto it = _ptrDestination.begin() ; it != _ptrDestination.end() ;)
	{
		const tree pointer = it->first;
		if (is_gimple_reg(pointer))
			++it;
		else
			it = _ptrDestination.erase(it);
	}

}

bool Configuration::tryAddConstraint(Constraint c)
{
	debug() << "Trying to add a constraint about " << strForTree(c.lhs)
		  << "\n\tlhs tree code: " << tree_code_name[TREE_CODE(c.lhs)]
		  << "\n\trhs tree code: " << tree_code_name[TREE_CODE(c.rhs)]
		  << std::endl;

	c.lhs = STRIP_USELESS_TYPE_CONVERSION(c.lhs);
	c.rhs = STRIP_USELESS_TYPE_CONVERSION(c.rhs);

	//Special case : int* p; int v; p = &v;
	//we want to remember that *p is aliased to v
	if (POINTER_TYPE_P(TREE_TYPE(c.lhs)) &&
	    c.rel == EQ_EXPR &&
	    TREE_CODE(c.rhs) == ADDR_EXPR) {
		_ptrDestination.emplace(c.lhs,TREE_OPERAND(c.rhs,0));
		return true;
	}

	if (!is_gimple_variable(c.lhs) ||
	    !(is_gimple_variable(c.rhs) || TREE_CODE(c.rhs) == INTEGER_CST)) {
		debug() << "Bad nodes" << std::endl;
		return false;
	}

	if ((DECL_P(c.lhs) && is_global_var(c.lhs)) ||
            (DECL_P(c.rhs) && is_gimple_variable(c.rhs) && is_global_var(c.rhs))) {
		debug() << "Cannot handle global vars" << std::endl;
		return false;
	}

	if ((DECL_P(c.lhs) && TREE_THIS_VOLATILE(c.lhs)) ||
	    (DECL_P(c.rhs) && TREE_THIS_VOLATILE(c.rhs))) {
		debug() << "Cannot handle volatile" << std::endl;
		return false;
	}

	debug() << "Constraint accepted" << std::endl;

	doAddConstraint(c);

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
		if (!name || name == NULL_TREE || strlen(IDENTIFIER_POINTER(name)) == 0)
			res = "<ssa " + std::to_string(ssaCounter++) + ">";
		else
			res = IDENTIFIER_POINTER(name);
		res += "." + std::to_string(SSA_NAME_VERSION(t));
	} else if (TREE_CODE(t) == VAR_DECL) {
		tree name = DECL_NAME(t);
		if (!name || name == NULL_TREE || strlen(IDENTIFIER_POINTER(name)) == 0)
			res = "<var " + std::to_string(varCounter++) + ">";
		else
			res = IDENTIFIER_POINTER(name);
	} else {
		debug() << "Warning: trying to get a name for " << tree_code_name[TREE_CODE(t)] << std::endl;
	}

	return _strings.insert(std::make_pair(t,res)).first->second;
}

term_t Configuration::getNormalizedTerm(tree t)
{
	assert (t && t != NULL_TREE);
	term_t res = NULL_TERM;
	if (is_gimple_variable(t)) {
		std::string s = strForTree(t);
		res = yices_get_term_by_name(s.c_str());
		if (res == NULL_TERM) {
			res = yices_new_uninterpreted_term(YICES_INT);
			yices_set_term_name(res, s.c_str());
		}
	} else if (TREE_CODE(t) == INTEGER_CST) {
		res = yices_int64(TREE_INT_CST(t).to_shwi());
	}
	debug() << "Normalized term " << strForTree(t) << std::endl;
	yices_pp_term(stderr, res, 120, 50, 0);

	return res;
}

void Configuration::setPredecessorInfo(RichBasicBlock* rbb, unsigned int edgeTaken)
{
	_preds.push_back(rbb);
	_indexLastEdgeTaken = edgeTaken;
}

void Configuration::printPath(std::ostream& out)
{
	out << "[";
	auto it = _preds.begin();
	if (it != _preds.end())
		out << **it;
	for (++it; it != _preds.end() ; ++it)
		out << ", " << **it;
	out << "]";
}
