#include <cstdlib>
#include <gcc-plugin.h>
#include <basic-block.h>
#include <tree-flow.h>

#include <assert.h>

#include <tuple>

#include "constraint.h"


Constraint::Constraint(tree lhs, tree_code rel, tree rhs) :
	lhs(lhs),
	rel(rel),
	rhs(rhs)
{}

Constraint::Constraint(edge e)
{
	basic_block src = e->src;
	if (e->flags & (EDGE_TRUE_VALUE | EDGE_FALSE_VALUE)) {
		gimple last = last_stmt(src);
		assert(gimple_code(last) == GIMPLE_COND);
		lhs = gimple_cond_lhs(last);
		rhs = gimple_cond_rhs(last);
		if (e->flags & EDGE_TRUE_VALUE)
			rel = gimple_cond_code(last);
		else
			rel = gimple_cond_code(last) == EQ_EXPR ? NE_EXPR :
			      gimple_cond_code(last) == NE_EXPR ? EQ_EXPR :
			      gimple_cond_code(last) == LT_EXPR ? GE_EXPR :
			      gimple_cond_code(last) == LE_EXPR ? GT_EXPR :
			      gimple_cond_code(last) == GT_EXPR ? LE_EXPR :
			   /* gimple_cond_code(last) == GE_EXPR ?*/ LT_EXPR;
	}
}

bool operator<(const Constraint& c1, const Constraint& c2)
{
	return std::tie(c1.lhs,c1.rel,c1.rhs) < std::tie(c2.lhs,c2.rel,c2.rhs);
}
