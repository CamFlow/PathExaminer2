#include <cstdlib>
#include <cstring>
#include <gcc-plugin.h>
#include <gimple.h>
#include <basic-block.h>

#include "rich_basic_block.h"

RichBasicBlock::RichBasicBlock(basic_block bb) :
	_bb(bb),
	_hasFlow(false),
	_hasLSM(false)
{
	if (bb == EXIT_BLOCK_PTR)
		return;

	std::cerr << "+ Building RichBasicBlock " << bb->index << std::endl;

	if (bb != ENTRY_BLOCK_PTR) {
		std::tie(_hasLSM,_hasFlow) = isLSMorFlowBB(_bb);
		std::cerr << std::boolalpha
			<< "    has flow: " << _hasFlow
			<< "    has LSM: "  << _hasLSM
			<< std::endl;
	}

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
		if (gimple_code(stmt) != GIMPLE_CALL)
			continue;
		std::cerr << "GIMPLE CALL" << std::endl;
		tree fndecl = gimple_call_fndecl(stmt);
		if (!fndecl || fndecl == NULL_TREE)
			continue;
		std::cerr << "GIMPLE CALL with a decl" << std::endl;

		tree fn = DECL_NAME(fndecl);
		if (fn && fn != NULL_TREE) {
			std::string name(IDENTIFIER_POINTER(fn));
			isLSM = isLSM || (name.find("security_") != name.npos);
			isFlow = isFlow || (name == "kayrebt_FlowNodeMarker");
			std::cerr << "name: " << name;
			std::cerr << "\tisLSM: " << isLSM;
			std::cerr << "\tisFlow: " << isFlow << std::endl;
		}
	}

	return std::make_tuple(isLSM, isFlow);
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

