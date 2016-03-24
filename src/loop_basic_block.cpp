#include <cstdlib>
#include <gcc-plugin.h>
#include <tree-flow.h>
#include <basic-block.h>
#include <cfgloop.h>

#include <set>

#include "loop_basic_block.h"
#include "configuration.h"

LoopBasicBlock::LoopBasicBlock(struct loop* l) :
	RichBasicBlock()
{
	basic_block* loopBBs = get_loop_body_in_dom_order(l);

	for (unsigned int i = 0 ; i < l->num_nodes ; i++) {
		basic_block bb = loopBBs[i];
		for (gimple_stmt_iterator it = gsi_start_phis(bb) ;
			!gsi_end_p(it);
			gsi_next(&it)) {
			gimple stmt = gsi_stmt(it);
			_clobbered.insert(gimple_phi_result(stmt));
		}
		for (gimple_stmt_iterator it = gsi_start_bb(bb) ;
			!gsi_end_p(it);
			gsi_next(&it)) {
			gimple stmt = gsi_stmt(it);
			tree lhs;
			switch (gimple_code(stmt)) {
				case GIMPLE_ASSIGN:
					lhs = gimple_assign_lhs(stmt);
					if (!_clobbersMemVars)
						_clobbersMemVars = POINTER_TYPE_P(TREE_TYPE(lhs));
					_clobbered.insert(lhs);
					break;
				case GIMPLE_CALL:
					lhs = gimple_call_lhs(stmt);
					if (lhs && lhs != NULL_TREE)
						_clobbered.insert(lhs);
					//fallthrough
				case GIMPLE_ASM:
					_clobbersMemVars = true;
					break;

				// control flow stmts are irrelevant
				case GIMPLE_COND:
				case GIMPLE_LABEL:
				case GIMPLE_GOTO:
				case GIMPLE_SWITCH:
					break;

				default:
					break;
			}
		}
	}

	std::vector<std::pair<basic_block,Constraint>> exits;
	unsigned int i;
	edge e;
	vec<edge> exitEdges = get_loop_exit_edges(l);
	FOR_EACH_VEC_ELT(exitEdges, i, e)
		if (e->dest != l->header)
			_succs.emplace(e->dest, std::forward_as_tuple(e,Constraint(e)));

	_loopBody = loopBBs;
	_size = l->num_nodes;
	_bb = loopBBs[0];
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

void LoopBasicBlock::applyAllConstraints(Configuration& k)
{
	for (tree t : _clobbered)
		k.resetVar(t);
	if (_clobbersMemVars)
		k.resetAllVarMem();
}
