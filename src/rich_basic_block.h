#include <gcc-plugin.h>
#include <basic-block.h>
#include <tree.h>
#include <gimple.h>

#include <tuple>
#include <vector>

#include "constraint.h"

class RichBasicBlock {
private:
	basic_block _bb;
	std::vector<std::pair<basic_block,Constraint>> _succs;
	struct {
		bool has_flow : 1;
		bool has_lsm  : 1;
	} _status;
	bool matchLSM(tree fndecl) const;
	bool matchFlow(gimple stmt) const;

public:
	RichBasicBlock(basic_block bb);
	bool hasFlowNode() const { return _status.has_flow; }
	bool hasLSMNode() const { return _status.has_lsm; }
	const basic_block& getRawBB() const;
};
