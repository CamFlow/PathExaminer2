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

class RichBasicBlock {
protected:
	basic_block _bb;
	std::map<basic_block,std::tuple<edge,Constraint>> _succs;
	bool _hasFlow;
	bool _hasLSM;
	static std::tuple<bool,bool> isLSMorFlowBB(basic_block bb);
	virtual void print(std::ostream& o) const;

	RichBasicBlock() = default;

public:
	explicit RichBasicBlock(basic_block bb);
	virtual ~RichBasicBlock() = default;
	bool hasFlowNode() const { return _hasFlow; }
	bool hasLSMNode() const { return _hasLSM; }
	const basic_block& getRawBB() const { return _bb; }
	std::tuple<const edge,const Constraint&> getConstraintForSucc(const RichBasicBlock& succ) const;

friend bool operator==(const RichBasicBlock&, const RichBasicBlock&);
friend std::ostream& operator<<(std::ostream& o, const RichBasicBlock& rbb);
};

#endif /* ifndef RICH_BASIC_BLOCK_H */
