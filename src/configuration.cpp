
#include <gcc-plugin.h>
#include <gimple.h>
#include <tree.h>

#include <memory>

#include "configuration.h"
#include "constraint.h"
#include "value.h"
#include "value_factory.h"


Configuration& operator<<(Configuration& k, gimple stmt)
{
	switch (gimple_code(stmt)) {
		default:
			; //nothing to do
	}
	return k;
}

Configuration& operator<<(Configuration& k, const Constraint& c)
{
	switch (c.rel) {
		default:
			; //we should probably raise an exception here
			  //it means that we don't handle one of the operator
			  //or that we mis-parsed the constraint
	}
	return k;
}


bool ValueRange::checkVectorOfConstraints(std::vector<term_t>& constraints)
{
		term_t conjunct = yices_and(constraints.size(), constraints.data());
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
		for (term_t& t : p.second._constraints)
			all_constraints.emplace_back(t);
	for (auto&& p : _varsTemp)
		for (term_t& t : p.second._constraints)
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

void Configuration::resetVarMem(tree varMem) {
	auto it = _varsMem.find(varMem);
	if (it == _varsMem.end())
		return;

	it->second._constraints.clear();
}

void Configuration::resetAllVarMem() {
	for (auto& p : _varsMem)
		p.second._constraints.clear();
}

void Configuration::addConstraint(const Constraint& c) {
	ValueRange& r = getValueRangeForVar(c.lhs);
	const std::map<tree_code, term_t(*)(term_t,term_t)> ops{
			{GT_EXPR,  yices_arith_gt_atom},
			{LT_EXPR,  yices_arith_lt_atom},
			{GE_EXPR,  yices_arith_geq_atom},
			{LE_EXPR,  yices_arith_leq_atom},
			{EQ_EXPR,  yices_arith_eq_atom},
			{NE_EXPR,  yices_arith_neq_atom}
	};

	if (is_gimple_variable(c.rhs))
		r._constraints.push_back(
			ops.at(c.rel)(
				yices_get_term_by_name(strForTree(c.lhs).c_str()),
				yices_get_term_by_name(strForTree(c.rhs).c_str()))
		);
	else if (TREE_CODE(c.rhs) == INTEGER_CST)
		r._constraints.push_back(
			ops.at(c.rel)(
				yices_get_term_by_name(strForTree(c.lhs).c_str()),
				yices_int64(TREE_INT_CST_HIGH(c.rhs) << 32) + TREE_INT_CST_LOW(c.rhs))
		);
	// otherwise we don't know what lhs is compared to
}

const std::string& Configuration::strForTree(tree t)
{
	auto it = _strings.find(t);
	if (it != _strings.end())
		return it->second;

	std::shared_ptr<Value> v = ValueFactory::INSTANCE.build(t);
	return _strings.emplace(t,v->print()).first->second;
}
