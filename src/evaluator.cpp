#include <cstdlib>
#include <cassert>
#include <gcc-plugin.h>
#include <function.h>
#include <tree-flow.h>
#include <basic-block.h>
#include <cfgloop.h>

#include <map>
#include <functional>
#include <memory>
#include <stack>
#include <set>

#include "evaluator.h"
#include "configuration.h"
#include "loop_basic_block.h"

Evaluator::Evaluator()
{
	basic_block bb;
	FOR_ALL_BB(bb) {
		if (bb == EXIT_BLOCK_PTR) //we need ENTRY but not EXIT
			continue;

		RichBasicBlock& rbb = bb->loop_father ?
			buildLoop(bb) :
			_allbbs.emplace(bb,bb).first->second;

		if (rbb.hasFlowNode())
			_bbsWithFlows.push_back(rbb);
		if (rbb.hasLSMNode())
			_bbsWithFlows.push_back(rbb);
	}
}

void Evaluator::evaluateAllPaths()
{
	for (RichBasicBlock& flowBB : _bbsWithFlows) {
		_graph.clear();
		buildSubGraph(flowBB);
		walkGraph(flowBB);
	}
}

LoopBasicBlock& Evaluator::buildLoop(basic_block bb)
{
	assert(bb_loop_depth(bb) > 0);

	struct loop* l = loop_outermost(bb->loop_father);
	basic_block* loopBBs = get_loop_body_in_dom_order(l);

	std::set<tree> clobberedVars;
	bool clobbersAllVarMems = 0;
	for (unsigned int i = 0 ; i < l->num_nodes ; i++) {
		basic_block bb = loopBBs[i];
		for (gimple_stmt_iterator it = gsi_start_phis(bb) ;
			!gsi_end_p(it);
			gsi_next(&it)) {
			gimple stmt = gsi_stmt(it);
			clobberedVars.insert(gimple_phi_result(stmt));
		}
		for (gimple_stmt_iterator it = gsi_start_bb(bb) ;
			!gsi_end_p(it);
			gsi_next(&it)) {
			gimple stmt = gsi_stmt(it);
			tree lhs;
			switch (gimple_code(stmt)) {
				case GIMPLE_ASSIGN:
					lhs = gimple_assign_lhs(stmt);
					clobbersAllVarMems = POINTER_TYPE_P(TREE_TYPE(lhs));
					clobberedVars.insert(lhs);
					break;
				case GIMPLE_CALL:
					lhs = gimple_call_lhs(stmt);
					if (lhs && lhs != NULL_TREE)
						clobberedVars.insert(lhs);
					//fallthrough
				case GIMPLE_ASM:
					clobbersAllVarMems = true;
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
		exits.emplace_back(e->dest, Constraint(e));
	LoopBasicBlock& lbb = static_cast<LoopBasicBlock&>(_allbbs.emplace(
			l->header,
			LoopBasicBlock(loopBBs, l->num_nodes,
				std::move(clobberedVars), std::move(exits))
			).first->second);
	if (clobbersAllVarMems)
		lbb.setClobbersAllMemVars();
	return lbb;
}

void Evaluator::buildSubGraph(RichBasicBlock& start)
{
	std::map<std::reference_wrapper<RichBasicBlock>,Color,RichBasicBlockLess> colors;

	for (auto&& rbb : _allbbs)
		if (rbb.first == ENTRY_BLOCK_PTR)
			colors[rbb.second] = Color::GREEN;
		else if (rbb.second.hasLSMNode())
			colors[rbb.second] = Color::RED;
		else
			colors[rbb.second] = Color::WHITE;

	dfs_visit(start, colors);
}

	// The resulting subgraph is the subgraph comprising the root node,
	// the starting node and every nodes and edges belonging to paths
	// from the root to the starting node that do not contain any RED node
void Evaluator::dfs_visit(RichBasicBlock& bb, std::map<std::reference_wrapper<RichBasicBlock>,Color,RichBasicBlockLess>& colors)
{
	colors[bb] = Color::GRAY;
	bool at_least_one_pred_green = false;
	edge e;
	edge_iterator it;

	// GREEN : node we want in the result subgraph
	// RED : node we don't want
	// WHITE : node undiscovered
	// GRAY : node discovered, but with predecessors undiscovered yet
	// The root node is GREEN, some nodes are RED, the starting node is WHITE
	FOR_EACH_EDGE(e,it,bb.getRawBB()->preds) {
		basic_block pred = e->src;
		// if we are in a loop, we must jump to the header
		// to fetch the LoopBasicBlock
		if (pred->loop_father)
			pred = loop_outermost(pred->loop_father)->header;

		auto p = _allbbs.at(pred); //better to just crash at this point
		                           //if p is NOT in the map

		// if the node we just discovered is new, then we must explore
		// it first before deciding the state of the current node
		if (colors[p] == Color::WHITE)
			dfs_visit(p, colors);
		// if we point to a GREEN node (e.g. the root, then we are
		// ourselves a GREEN node)
		// we keep also edges pointing to GRAY nodes (edges poiting
		// backward in the path we are currently exploring)
		// we remove them in a second pass if they're not interesting
		if (colors[p] == Color::GREEN ||
		    colors[p] == Color::GRAY) {
			auto succs = _graph.find(p);
			if (succs == _graph.cend())
				succs = _graph.emplace(p,std::vector<RichBasicBlock>()).first;
			succs->second.push_back(bb);
			at_least_one_pred_green = true;
		}
		if (at_least_one_pred_green)
			colors[bb] = Color::GREEN;
		else
			colors[bb] = Color::RED;
	}

	// Remove backward edges in a second pass if we now can decide that they
	// point to a RED node
	for (auto& p: _graph) {
		auto it = p.second.rbegin();
		auto save = it;
		while (it != p.second.rend()) {
			if (colors[*it] == Color::RED) {
				save = it;
				++save;
				p.second.erase(it.base());
				it = save;
			} else {
				++it;
			}
		}
	}
}

void Evaluator::walkGraph(RichBasicBlock& dest)
{
	std::stack<std::pair<RichBasicBlock&,Configuration>> walk;
	walk.emplace(_allbbs.at(ENTRY_BLOCK_PTR),Configuration());
	while (!walk.empty()) {
		RichBasicBlock& rbb = walk.top().first;
		Configuration& k = walk.top().second;
		walk.pop();

		if (rbb == dest) {
			; //TODO do something because we have found a possible path
		}

		for (gimple_stmt_iterator it = gsi_start_phis(rbb.getRawBB()) ;
			!gsi_end_p(it);
			gsi_next(&it)) {
			gimple stmt = gsi_stmt(it);
			k << stmt;
		}
		for (gimple_stmt_iterator it = gsi_start_bb(rbb.getRawBB()) ;
			!gsi_end_p(it);
			gsi_next(&it)) {
			gimple stmt = gsi_stmt(it);
			k << stmt;
		}

		for (auto&& succ : _graph[rbb]) { //for all successors of current bb
			const Constraint& c = rbb.getConstraintForSucc(succ);
			Configuration newk{k};

			// find the index of the edge in succ->pred we are
			// traversing to get from rbb to succ
			// This is needed to correctly interpret the Phi nodes
			// in succ
			edge_iterator it;
			edge e;
			unsigned int edgeIndex = 0;
			FOR_EACH_EDGE(e, it, succ.getRawBB()->preds) {
				if (e->src == rbb.getRawBB())
					break;
				edgeIndex++;
			}
			newk.setPredecessorInfo(rbb.getRawBB(),edgeIndex);
			newk << c;
			if (newk)
				walk.emplace(succ, std::move(newk));
			// else : abandon the path, the resulting configuration is invalid
		}
	}
}
