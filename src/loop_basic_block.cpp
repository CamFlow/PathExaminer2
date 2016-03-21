#include <cstdlib>
#include <gcc-plugin.h>

#include <set>

#include "loop_basic_block.h"

LoopBasicBlock::LoopBasicBlock(basic_block* loopBody, size_t size, std::set<tree>&& clobberedVars, std::vector<std::pair<basic_block,Constraint>>&& exitConstraints) :
	RichBasicBlock(),
	_loopBody(loopBody),
	_size(size),
	_clobbered(clobberedVars)
{
	for (auto&& p : exitConstraints)
		_succs.insert(p);

	_bb = loopBody[0];
	_hasLSM =false;
	_hasFlow = false;

	for (unsigned int i=0 ; i<_size ; i++) {
		bool lsm, flow;
		std::tie(lsm,flow) = isLSMorFlowBB(_loopBody[i]);
		_hasLSM = _hasLSM || lsm;
		_hasFlow = _hasFlow || flow;
	}
}

LoopBasicBlock::~LoopBasicBlock()
{
	free(_loopBody);
}

void LoopBasicBlock::print(std::ostream& o) const
{
	o << "(LoopBasicBlock)";
	this->RichBasicBlock::print(o);
}
