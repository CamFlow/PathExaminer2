#include <cstdlib>
#include <gcc-plugin.h>
#include <basic-block.h>
#include <tree.h>
#include <gimple.h>

#include <iostream>

#include "loop_header_basic_block.h"
#include "configuration.h"
#include "debug.h"

LoopHeaderBasicBlock::LoopHeaderBasicBlock(basic_block bb) : RichBasicBlock(bb)
{
}

void LoopHeaderBasicBlock::applyAllConstraints(Configuration& k)
{
	// we discard PHI statements in loop headers
	for (gimple_stmt_iterator it = gsi_start_bb(_bb) ;
			!gsi_end_p(it);
			gsi_next(&it)) {
		gimple stmt = gsi_stmt(it);
		debug() << "Next statement : " << gimple_code_name[gimple_code(stmt)] << std::endl;
		k << stmt;
	}
}

void LoopHeaderBasicBlock::print(std::ostream& o) const
{
	o << "(Loop header) ";
	RichBasicBlock::print(o);
}
