#include <cstdlib>
#include <gcc-plugin.h>
#include <basic-block.h>
#include <tree-flow.h>

#include <assert.h>
#include "constraint.h"
#include "value.h"
#include "value_factory.h"

Constraint::Constraint(edge e)
{
	basic_block src = e->src;
	if (e->flags & (EDGE_TRUE_VALUE | EDGE_FALSE_VALUE)) {
		gimple last = last_stmt(src);
		assert(gimple_code(last) == GIMPLE_COND);
		rel = gimple_cond_code(last);
		lhs = ValueFactory::INSTANCE.build(gimple_cond_lhs(last))->print();
		rhs = ValueFactory::INSTANCE.build(gimple_cond_rhs(last))->print();
	}
}
