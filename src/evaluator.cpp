#include <cstdlib>
#include <cassert>
#include <gcc-plugin.h>
#include <function.h>
#include <tree-flow.h>
#include <basic-block.h>
#include <cfgloop.h>

#include <iostream>
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
	std::cerr << "Building the rich basic blocks" << std::endl;
	basic_block bb;
	FOR_ALL_BB(bb) {
		std::cerr << "About to build basic block " << bb->index << std::endl;
		std::cerr << "is it a loop? " << std::boolalpha
			  << bool(bb_loop_depth(bb) > 0) << std::endl;

		RichBasicBlock* rbb =
			(bb_loop_depth(bb) > 0) ?
			buildLoop(bb) :
			_allbbs.emplace(bb, std::unique_ptr<RichBasicBlock>(new RichBasicBlock(bb)))
				.first->second.get();

		if (rbb->hasFlowNode())
			_bbsWithFlows.insert(rbb);
	}
}

void Evaluator::evaluateAllPaths()
{
	std::cerr << "There are " << _bbsWithFlows.size() << " bbs with flow nodes" << std::endl;
	for (RichBasicBlock* flowBB : _bbsWithFlows) {
		std::cerr << "Examining " << *flowBB << std::endl;
		_graph.clear();
		buildSubGraph(flowBB);
		std::cerr << "These are all the basic blocks:" << std::endl;
		for (const auto& p : _allbbs) {
			std::cerr << *(p.second) << std::endl;
		}
		std::cerr << "These are the basic blocks from the interesting "
			     "subgraph:" << std::endl;
		for (const auto& p : _graph) {
			std::cerr << '['
				  << p.first->getRawBB()->index
				  << "] (succs in graph: ";
			for (const auto& s : p.second)
				std::cerr << '['
					  << s->getRawBB()->index
					  << "] ";
			std::cerr << ")" << std::endl;
		}
//		walkGraph(flowBB);
	}
}

LoopBasicBlock* Evaluator::buildLoop(basic_block bb)
{
	assert(bb_loop_depth(bb) > 0);

	std::cerr << "++ Building a loop pseudo-basic block" << std::endl;
	struct loop* l = loop_outermost(bb->loop_father);
	basic_block header = l->header;
	auto it = _allbbs.find(header);
	if (it != _allbbs.end())
		return static_cast<LoopBasicBlock*>(it->second.get());

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
	LoopBasicBlock* lbb = static_cast<LoopBasicBlock*>(_allbbs.emplace(
			l->header,
			std::unique_ptr<RichBasicBlock>(
				new LoopBasicBlock(loopBBs, l->num_nodes,
				std::move(clobberedVars), std::move(exits)))
			).first->second.get());
	std::cerr << "Loop added for basic_block " << l->header->index << " (header)" << std::endl;
	if (clobbersAllVarMems)
		lbb->setClobbersAllMemVars();
	return lbb;
}

void Evaluator::buildSubGraph(RichBasicBlock* start)
{
	std::map<RichBasicBlock*,Color> colors;

	for (const auto& rbb : _allbbs)
		if (rbb.first == ENTRY_BLOCK_PTR)
			colors[rbb.second.get()] = Color::GREEN;
		else if (rbb.second->hasLSMNode())
			colors[rbb.second.get()] = Color::RED;
		else
			colors[rbb.second.get()] = Color::WHITE;

	dfs_visit(start, colors);
	if (colors[start] == Color::GREEN)
		// we finish the algorithm by including the starting BB
		// with no successors
		_graph.emplace(start,std::vector<RichBasicBlock*>());
}

	// The resulting subgraph is the subgraph comprising the root node,
	// the starting node and every nodes and edges belonging to paths
	// from the root to the starting node that do not contain any RED node
void Evaluator::dfs_visit(RichBasicBlock* bb, std::map<RichBasicBlock*,Color>& colors)
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
	FOR_EACH_EDGE(e,it,bb->getRawBB()->preds) {
		basic_block pred = e->src;
		std::cerr << "basic block " << pred->index
			  << " is a predecessor" << std::endl;
		// if we are in a loop, we must jump to the header
		// to fetch the LoopBasicBlock
		if (bb_loop_depth(pred) > 0) {
			std::cerr << "++ Visiting a loop bb" << std::endl;
			pred = loop_outermost(pred->loop_father)->header;
			std::cerr << "++ jumping to the header of the loop: " << pred->index << std::endl;
			if (pred == bb->getRawBB()) {
				//hmm pred is a basic block inside the loop
				//headed by bb
				//probably because we've just gone through the
				//latch, we can cut the current branch of
				//exploration here and explore the other preds
				std::cerr << "Stuck in a loop, abandoning this "
					     "path"
					  << std::endl;
				continue;
			}
		}

		//better to just crash at this point if p is NOT in the map
		RichBasicBlock* p = _allbbs.at(pred).get();

		std::cerr << "++ got the corresponding RichBasicBlock" << std::endl;

		// if the node we just discovered is new, then we must explore
		// it first before deciding the state of the current node
		if (colors[p] == Color::WHITE) {
			std::cerr << *p << " is white, visiting it first" << std::endl;
			dfs_visit(p, colors);
		}
		// if we point to a GREEN node (e.g. the root, then we are
		// ourselves a GREEN node)
		if (colors[p] == Color::GREEN ||
		    colors[p] == Color::GRAY) {
			std::cerr << *p << " is green (or gray), adding myself as a successor" << std::endl;
			auto succs = _graph.find(p);
			if (succs == _graph.cend())
				succs = _graph.emplace(p,std::vector<RichBasicBlock*>()).first;
			succs->second.push_back(bb);
			at_least_one_pred_green = true;
		}

		std::cerr << p << " is " << ((colors[p] == Color::GREEN) ? "green" : "red") << std::endl;
		if (at_least_one_pred_green) {
			colors[bb] = Color::GREEN;
		} else {
			colors[bb] = Color::RED;
		}
	}
}

void Evaluator::walkGraph(RichBasicBlock* dest)
{
	std::stack<std::pair<RichBasicBlock*,Configuration>> walk;
	walk.emplace(_allbbs.at(ENTRY_BLOCK_PTR).get(),Configuration());
	while (!walk.empty()) {
		RichBasicBlock* rbb = walk.top().first;
		Configuration& k = walk.top().second;
		walk.pop();

		if (rbb == dest) {
			; //TODO do something because we have found a possible path
		}

		for (gimple_stmt_iterator it = gsi_start_phis(rbb->getRawBB()) ;
			!gsi_end_p(it);
			gsi_next(&it)) {
			gimple stmt = gsi_stmt(it);
			k << stmt;
		}
		for (gimple_stmt_iterator it = gsi_start_bb(rbb->getRawBB()) ;
			!gsi_end_p(it);
			gsi_next(&it)) {
			gimple stmt = gsi_stmt(it);
			k << stmt;
		}

		for (const auto& succ : _graph[rbb]) { //for all successors of current bb
			const Constraint& c = rbb->getConstraintForSucc(*succ);
			Configuration newk{k};

			// find the index of the edge in succ->pred we are
			// traversing to get from rbb to succ
			// This is needed to correctly interpret the Phi nodes
			// in succ
			edge_iterator it;
			edge e;
			unsigned int edgeIndex = 0;
			FOR_EACH_EDGE(e, it, succ->getRawBB()->preds) {
				if (e->src == rbb->getRawBB())
					break;
				edgeIndex++;
			}
			newk.setPredecessorInfo(rbb->getRawBB(),edgeIndex);
			newk << c;
			if (newk)
				walk.emplace(succ, std::move(newk));
			// else : abandon the path, the resulting configuration is invalid
		}
	}
}
