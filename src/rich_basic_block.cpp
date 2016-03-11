#include <cstdlib>
#include <gcc-plugin.h>
#include <gimple.h>
#include <basic-block.h>

#include "rich_basic_block.h"

RichBasicBlock::RichBasicBlock(basic_block bb) :
	_bb(bb)
{
	if (bb == ENTRY_BLOCK_PTR || bb == EXIT_BLOCK_PTR)
		return;

	std::tie(_hasLSM,_hasFlow) = isLSMorFlowBB(_bb);

	edge succ;
	edge_iterator succ_it;
	FOR_EACH_EDGE(succ, succ_it, _bb->succs)
		_succs.emplace(succ->src, Constraint(succ));
}

std::tuple<bool,bool> RichBasicBlock::isLSMorFlowBB(basic_block bb)
{
	bool isLSM = false;
	bool isFlow = false;
	for (gimple_stmt_iterator it = gsi_start_bb(bb) ;
		!gsi_end_p(it) ;
		gsi_next(&it)) {
		gimple stmt = gsi_stmt(it);
		if (gimple_code(stmt) == GIMPLE_CALL) {
			isLSM = matchLSM(gimple_call_fndecl(stmt));
		}
		isFlow = matchFlow(stmt);
	}

	return std::make_tuple(isLSM, isFlow);
}

bool matchLSM(tree fndecl)
{
	if (!fndecl)
		return false;

	tree fn = DECL_NAME(fndecl);
	if (fn && fn != NULL_TREE) {
		std::string name(IDENTIFIER_POINTER(fn));
		return name.find("security_") == 0; //LSM hooks start with "security_"
	}

	return false;
}

bool matchFlow(gimple stmt)
{
	// ??
	return stmt;
}

bool operator==(const RichBasicBlock& bb1, const RichBasicBlock& bb2)
{
	return bb1._bb == bb2._bb;
}

const Constraint& RichBasicBlock::getConstraintForSucc(const RichBasicBlock& succ) const
{
	return _succs.at(succ.getRawBB()); //throws an error if bb is not found
					   //it should NEVER be the case
}
