/**
 * @file loop_basic_block.h
 * @brief Definition of the LoopBasicBlock class
 * @author Laurent Georget
 * @version 0.1
 * @date 2016-03-25
 */
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

/**
 * @brief A loop basic block is a rich basic block representing an entire loop
 * with all its basic blocks
 *
 * The underlying GCC basic block that distinguishes this kind of rich basic
 * block is the header of the loop.
 */
class LoopBasicBlock : public RichBasicBlock
{
private:
	/**
	 * @brief The basic blocks inside the loop
	 */
	basic_block* _loopBody;
	/**
	 * @brief The set of variables defined inside the body of the loop
	 */
	std::set<tree> _clobbered;
	/**
	 * @brief Whether this loop contains at least one virtual definition
	 */
	bool _clobbersMemVars = false;

public:
	/**
	 * @brief Builds a loop basic block from a GCC struct loop
	 * @param l a GCC loop
	 */
	LoopBasicBlock(struct loop* l);
	virtual void print(std::ostream& o) const override;
	virtual void applyAllConstraints(Configuration& k) override;
};

#endif /* ifndef LOOP_BASIC_BLOCK_H */
