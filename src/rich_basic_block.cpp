/**
 * @file rich_basic_block.cpp
 * @brief Implementation of the RichBasicBlock class
 * @author Laurent Georget, Michael Han
 * @version 0.2
 * @date 2019-11-06
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
	_hasLSM(false),
	_hasFunc(false)
{
	if (bb == EXIT_BLOCK_PTR)
		return;

	debug() << "Building a regular RichBasicBlock " << bb->index << std::endl;

	if (bb != ENTRY_BLOCK_PTR) {
		std::tie(_hasLSM,_hasFunc) = isLSMorFuncBB(_bb);
		debug() << std::boolalpha
			<< "    has LSM: "  << _hasLSM
			<< "	has func call (other than LSM): " << _hasFunc
			<< std::endl;
	}

	edge succ;
	edge_iterator succ_it;
	FOR_EACH_EDGE(succ, succ_it, _bb->succs)
		_succs.emplace(succ->dest, std::forward_as_tuple(succ, Constraint(succ)));
	
	edge pred;
	edge_iterator pred_it;
	FOR_EACH_EDGE(pred, pred_it, _bb->preds)
		_preds.emplace(pred->src, std::forward_as_tuple(pred, Constraint(pred)));		
}

std::tuple<bool,bool> RichBasicBlock::isLSMorFuncBB(basic_block bb)
{
	bool isLSM = false;
	bool isFunc = false;
	for (gimple_stmt_iterator it = gsi_start_bb(bb) ;
		!gsi_end_p(it) ;
		gsi_next(&it)) {
		gimple stmt = gsi_stmt(it);
		if (gimple_code(stmt) != GIMPLE_CALL)
			continue;
		// debug() << "GIMPLE CALL" << std::endl;
		tree fndecl = gimple_call_fndecl(stmt);
		if (!fndecl || fndecl == NULL_TREE)
			continue;
		// debug() << "GIMPLE CALL with a decl" << std::endl;

		tree fn = DECL_NAME(fndecl);
		if (fn && fn != NULL_TREE) {
			std::string name(IDENTIFIER_POINTER(fn));
			isLSM = isLSM || (name.find("security_") != name.npos);
			isFunc = isFunc || (name.find("security_") == name.npos);
			debug() << "name: " << name;
			debug() << "\n";
			_funcNames.push_back(name);
		}
	}
	debug() << "Block has LSM: " << isLSM;
	debug() << "\tBlock has isFunc: " << isFunc;
	debug() << "\n";

	return std::make_tuple(isLSM, isFunc);
}

std::tuple<const edge,const Constraint&> RichBasicBlock::getConstraintForSucc(const RichBasicBlock& succ) const
{
	return _succs.at(succ.getRawBB()); //throws an error if bb is not found
					   //it should NEVER be the case
}

std::tuple<const edge,const Constraint&> RichBasicBlock::getConstraintForPred(const RichBasicBlock& pred) const
{
        return _preds.at(pred.getRawBB()); //throws an error if bb is not found
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
