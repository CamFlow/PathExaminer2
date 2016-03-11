#ifndef LOOP_BASIC_BLOCK_H
#define LOOP_BASIC_BLOCK_H

#include <gcc-plugin.h>
#include <basic-block.h>
#include <tree.h>
#include <gimple.h>

#include <set>
#include <vector>

#include "rich_basic_block.h"

class LoopBasicBlock : public RichBasicBlock {
private:
	basic_block* _loopBody;
	size_t _size;
	std::set<tree> _clobbered;
	bool _clobbersMemVars = false;

public:
	LoopBasicBlock(basic_block* loopBody, size_t size, std::set<tree>&& clobberedVars, std::vector<std::pair<basic_block,Constraint>>&& exitConstraints);
	~LoopBasicBlock();
	void setClobbersAllMemVars() { _clobbersMemVars = true; }
	bool getClobbersAllMemVars() { return _clobbersMemVars; }
};

#endif /* ifndef LOOP_BASIC_BLOCK_H */
