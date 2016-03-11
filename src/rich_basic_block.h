#ifndef RICH_BASIC_BLOCK_H
#define RICH_BASIC_BLOCK_H

#include <gcc-plugin.h>
#include <basic-block.h>
#include <tree.h>
#include <gimple.h>

#include <tuple>
#include <map>

#include "constraint.h"

class RichBasicBlock {
protected:
	basic_block _bb;
	std::map<basic_block,Constraint> _succs;
	bool _hasFlow;
	bool _hasLSM;
	static std::tuple<bool,bool> isLSMorFlowBB(basic_block bb);
	static bool matchLSM(tree fndecl);
	static bool matchFlow(gimple stmt);

	RichBasicBlock() = default;

public:
	RichBasicBlock(basic_block bb);
	bool hasFlowNode() const { return _hasFlow; }
	bool hasLSMNode() const { return _hasLSM; }
	const basic_block& getRawBB() const;
	const Constraint& getConstraintForSucc(const RichBasicBlock& succ) const;

friend bool operator==(const RichBasicBlock&, const RichBasicBlock&);
};

#endif /* ifndef RICH_BASIC_BLOCK_H */
