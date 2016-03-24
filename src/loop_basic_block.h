#ifndef LOOP_BASIC_BLOCK_H
#define LOOP_BASIC_BLOCK_H

#include <gcc-plugin.h>
#include <basic-block.h>
#include <tree.h>
#include <gimple.h>
#include <tree-flow.h>
#include <basic-block.h>
#include <cfgloop.h>

#include <set>
#include <vector>
#include <tuple>

#include "rich_basic_block.h"

class LoopBasicBlock : public RichBasicBlock {
private:
	basic_block* _loopBody;
	size_t _size;
	std::set<tree> _clobbered;
	bool _clobbersMemVars = false;

public:
	LoopBasicBlock(struct loop* l);
	~LoopBasicBlock();
	virtual void print(std::ostream& o) const override;
	void setClobbersAllMemVars() { _clobbersMemVars = true; }
	bool getClobbersAllMemVars() { return _clobbersMemVars; }
	virtual void applyAllConstraints(Configuration& k) override;
};

#endif /* ifndef LOOP_BASIC_BLOCK_H */
