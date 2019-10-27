/**
 * @file rich_basic_block.h
 * @brief Definition of the RichBasicBlock class
 * @author Laurent Georget
 * @version 0.1
 * @date 2016-03-25
 */
#ifndef RICH_BASIC_BLOCK_H
#define RICH_BASIC_BLOCK_H

#include <gcc-plugin.h>
#include <basic-block.h>
#include <tree.h>
#include <gimple.h>

#include <iostream>
#include <tuple>
#include <map>

#include "constraint.h"

class Configuration;

/**
 * @brief A rich basic block is a wrapper around GCC's basic blocks to store
 * additional state
 */
class RichBasicBlock {
protected:
	/**
	 * @brief The underlying GCC basic block
	 */
	basic_block _bb;
	/**
	 * @brief The successors of the basic block in the CFG
	 */
	std::map<basic_block,std::tuple<edge,Constraint>> _succs;

	std::map<basic_block,std::tuple<edge,Constraint>> _preds;
	/**
	 * @brief Whether the basic block contains a flow instruction
	 */
	bool _hasFlow;
	/**
	 * @brief Whether the basic block contains a LSM hook
	 */
	bool _hasLSM;
	/**
	 * @brief Explore the basic block to see if it contains a flow
	 * instruction or a LSM hook
	 * @param bb the basic block to explore
	 * @return a pair of booleans (b1,b2), b1 is true is and only if bb
	 * contains a LSM hook, b2 is true if and only if bb contains a flow
	 * instruction
	 */
	static std::tuple<bool,bool> isLSMorFlowBB(basic_block bb);
	/**
	 * @brief Prints the index of the basic block
	 * @param o the output stream where to print the basic block
	 */
	virtual void print(std::ostream& o) const;

	/**
	 * @brief Default constructor which does nothing special, only useful
	 * for subclasses
	 */
	RichBasicBlock() = default;

public:
	/**
	 * @brief Builds a regular rich basic block connected to one GCC basic
	 * block
	 * @param bb the underlying basic block
	 */
	explicit RichBasicBlock(basic_block bb);
	/**
	 * @brief Virtual destructor for subclasses
	 */
	virtual ~RichBasicBlock() = default;
	/**
	 * @brief Tells whether this rich basic blocks contains a flow node
	 * @return true if, and only if, the basic block contains a flow node
	 */
	bool hasFlowNode() const { return _hasFlow; }
	/**
	 * @brief Tells whether this rich basic blocks contains a LSM hook
	 * @return true if, and only if, the basic block contains a LSM hook
	 */
	bool hasLSMNode() const { return _hasLSM; }
	/**
	 * @brief Gets the underlying basic block
	 * @return the underlying GCC basic block
	 */
	const basic_block& getRawBB() const { return _bb; }

	std::map<basic_block,std::tuple<edge,Constraint>>& getSuccs() { return _succs; } 
	
	std::map<basic_block,std::tuple<edge,Constraint>>& getPreds() { return _preds; } 
	/**
	 * @brief Update the configuration passed as a parameter with all the
	 * constraints bringed along by this basic block
	 * @param k the configuration to update
	 */
	virtual void applyAllConstraints(Configuration& k);

	/**
	 * @brief Gets the edge and the constraint associated to it given a
	 * basic block successor of the current basic block
	 * @param succ another basic block. An edge must exist from the current
	 * basic block to that one.
	 * @return a pair (edge,constraint) where edge is the edge in the CFG
	 * connecting the GCC basic blocks and constraint represents the
	 * constraint born by this edge
	 */
	std::tuple<const edge,const Constraint&> getConstraintForSucc(const RichBasicBlock& succ) const;

	std::tuple<const edge,const Constraint&> getConstraintForPred(const RichBasicBlock& pred) const;

/**
 * @brief Outputs a rich basic block to an output stream
 * @param o an output stream
 * @param rbb the rich basic block to print
 * @return the output stream taken as parameter
 */
friend std::ostream& operator<<(std::ostream& o, const RichBasicBlock& rbb);
};

#endif /* ifndef RICH_BASIC_BLOCK_H */
