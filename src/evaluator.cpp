#include <cstdlib>
#include <gcc-plugin.h>
#include <function.h>
#include <tree-flow.h>
#include <basic-block.h>

#include <map>
#include <functional>
#include <memory>
#include <stack>

#include "evaluator.h"
#include "configuration.h"

Evaluator::Evaluator()
{
	basic_block bb;
	FOR_ALL_BB(bb) {
		if (bb == EXIT_BLOCK_PTR) //we need ENTRY but not EXIT
			continue;

		RichBasicBlock& rbb = _allbbs.emplace(bb,bb).first->second;
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

	std::pair<const basic_block,RichBasicBlock> firstVisited = std::make_pair(start.getRawBB(), start);
	dfs_visit(firstVisited, colors);
}

	// The resulting subgraph is the subgraph comprising the root node,
	// the starting node and every nodes and edges belonging to paths
	// from the root to the starting node that do not contain any RED node
void Evaluator::dfs_visit(std::pair<const basic_block,RichBasicBlock>& bb, std::map<std::reference_wrapper<RichBasicBlock>,Color,RichBasicBlockLess> colors)
{
	colors[bb.second] = Color::GRAY;
	bool at_least_one_pred_green = false;
	edge e;
	edge_iterator it;

	// GREEN : node we want in the result subgraph
	// RED : node we don't want
	// WHITE : node undiscovered
	// GRAY : node discovered, but with predecessors undiscovered yet
	// The root node is GREEN, some nodes are RED, the starting node is WHITE
	FOR_EACH_EDGE(e,it,bb.first->preds) {
		auto p = _allbbs.find(e->src);
		// if the node we just discovered is new, then we must explore
		// it first before deciding the state of the current node
		if (colors[p->second] == Color::WHITE)
			dfs_visit(*p, colors);
		// if we point to a GREEN node (e.g. the root, then we are
		// ourselves a GREEN node)
		// we keep also edges pointing to GRAY nodes (edges poiting
		// backward in the path we are currently exploring)
		// we remove them in a second pass if they're not interesting
		if (colors[p->second] == Color::GREEN ||
		    colors[p->second] == Color::GRAY) {
			auto succs = _graph.find(p->second);
			if (succs == _graph.cend())
				succs = _graph.emplace(p->second,std::vector<RichBasicBlock>()).first;
			succs->second.push_back(bb.second);
			at_least_one_pred_green = true;
		}
		if (at_least_one_pred_green)
			colors[bb.second] = Color::GREEN;
		else
			colors[bb.second] = Color::RED;
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
			; //do something because we have found a possible path
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
			newk << c;
			if (newk)
				walk.emplace(succ, std::move(newk));
			// else : abandon the path, the resulting configuration is invalid
		}
	}
}
