#ifndef LOOP_HEADER_BASIC_BLOCK_H
#define LOOP_HEADER_BASIC_BLOCK_H

#include <gcc-plugin.h>
#include <basic-block.h>
#include <tree.h>
#include <gimple.h>

#include <iostream>

#include "rich_basic_block.h"

class LoopHeaderBasicBlock : public RichBasicBlock
{
public:
	explicit LoopHeaderBasicBlock(basic_block bb);
	virtual void applyAllConstraints(Configuration&) override;
	virtual void print(std::ostream& o) const override;
};

#endif /* LOOP_HEADER_BASIC_BLOCK_H */
