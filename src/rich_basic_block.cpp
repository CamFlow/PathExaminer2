/**
 * @file rich_basic_block.cpp
 * @brief Implementation of the RichBasicBlock class
 * @author Laurent Georget
 * @version 0.1
 * @date 2016-03-27
 */
#include <cstdlib>
#include <cstring>
#include <gcc-plugin.h>
#include <gimple.h>
#include <basic-block.h>
#include <tree-flow.h>

#include <tuple>

#include "rich_basic_block.h"
#include "configuration.h"
#include "debug.h"

RichBasicBlock::RichBasicBlock(basic_block bb) :
	_bb(bb),
	_hasFlow(false),
	_hasLSM(false)
{
	if (bb == EXIT_BLOCK_PTR)
		return;

	debug() << "Building a regular RichBasicBlock " << bb->index << std::endl;

	if (bb != ENTRY_BLOCK_PTR) {
		std::tie(_hasLSM,_hasFlow) = isLSMorFlowBB(_bb);
		debug() << std::boolalpha
			<< "    has flow: " << _hasFlow
			<< "    has LSM: "  << _hasLSM
			<< std::endl;
	}

	edge succ;
	edge_iterator succ_it;
	FOR_EACH_EDGE(succ, succ_it, _bb->succs)
		_succs.emplace(succ->dest, std::forward_as_tuple(succ, Constraint(succ)));
}

std::tuple<bool,bool> RichBasicBlock::isLSMorFlowBB(basic_block bb)
{
	bool isLSM = false;
	bool isFlow = false;
	for (gimple_stmt_iterator it = gsi_start_bb(bb) ;
		!gsi_end_p(it) ;
		gsi_next(&it)) {
		gimple stmt = gsi_stmt(it);
		if (gimple_code(stmt) != GIMPLE_CALL)
			continue;
		debug() << "GIMPLE CALL" << std::endl;
		tree fndecl = gimple_call_fndecl(stmt);
		if (!fndecl || fndecl == NULL_TREE)
			continue;
		debug() << "GIMPLE CALL with a decl" << std::endl;

		tree fn = DECL_NAME(fndecl);
		if (fn && fn != NULL_TREE) {
			std::string name(IDENTIFIER_POINTER(fn));
			isLSM = isLSM || (name.find("security_") != name.npos);
			isFlow = isFlow || (name == "kayrebt_FlowNodeMarker");
			debug() << "name: " << name;
			debug() << "\tisLSM: " << isLSM;
			debug() << "\tisFlow: " << isFlow << std::endl;
		}
	}

	return std::make_tuple(isLSM, isFlow);
}

std::tuple<const edge,const Constraint&> RichBasicBlock::getConstraintForSucc(const RichBasicBlock& succ) const
{
	return _succs.at(succ.getRawBB()); //throws an error if bb is not found
					   //it should NEVER be the case
}

std::ostream& operator<<(std::ostream& o, const RichBasicBlock& rbb)
{
	rbb.print(o);
	return o;
}

void RichBasicBlock::print(std::ostream& o) const
{
	o << '<' << _bb->index << '>';
	o << " (succs: ";
	edge elt;
	edge_iterator it;
	FOR_EACH_EDGE(elt, it, _bb->succs) {
		o << elt->dest->index << " ";
	}
	o << ")";
}

void RichBasicBlock::applyAllConstraints(Configuration& k)
{
	for (gimple_stmt_iterator it = gsi_start_phis(_bb) ;
			!gsi_end_p(it);
			gsi_next(&it)) {
		gimple stmt = gsi_stmt(it);
		k << stmt;
	}
	for (gimple_stmt_iterator it = gsi_start_bb(_bb) ;
			!gsi_end_p(it);
			gsi_next(&it)) {
		gimple stmt = gsi_stmt(it);
		debug() << "Next statement : " << gimple_code_name[gimple_code(stmt)] << std::endl;
		k << stmt;
	}
}
