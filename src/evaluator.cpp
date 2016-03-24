#include <cstdlib>
#include <cassert>
#include <gcc-plugin.h>
#include <function.h>
#include <tree-flow.h>
#include <basic-block.h>
#include <cfgloop.h>
#include <tree-ssa-alias.h>

#include <iostream>
#include <map>
#include <functional>
#include <memory>
#include <stack>
#include <set>

#include <yices.h>

#include "evaluator.h"
#include "configuration.h"
#include "loop_basic_block.h"

Evaluator::Evaluator()
{
	yices_init();
	compute_may_aliases(); //needed for the points-to oracle

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

Evaluator::~Evaluator()
{
	yices_exit();
}

void Evaluator::evaluateAllPaths()
{
	std::cerr << "There are " << _bbsWithFlows.size()
		  << " bbs with flow nodes" << std::endl;
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
		walkGraph(flowBB);
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

	LoopBasicBlock* lbb = static_cast<LoopBasicBlock*>(
		_allbbs.emplace(
			l->header,
			std::unique_ptr<RichBasicBlock>(
				new LoopBasicBlock(l))
		).first->second.get());

	std::cerr << "Loop added for basic_block " << l->header->index
		  << " (header)" << std::endl;
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
	std::cerr << "Starting the walk until " << *dest << std::endl;
	std::stack<std::pair<RichBasicBlock*,Configuration>> walk;
	walk.emplace(_allbbs.at(ENTRY_BLOCK_PTR).get(),Configuration());
	while (!walk.empty()) {
		RichBasicBlock* rbb = walk.top().first;
		std::cerr << "Reached " << *rbb << std::endl;
		Configuration k = std::move(walk.top().second);
		walk.pop();

		if (rbb == dest) {
			std::cerr << "Found a path" << std::endl;
			//TODO do something because we have found a possible path
		}

		rbb->applyAllConstraints(k);
		std::cerr << "Handled all statements" << std::endl;

		for (const auto& succ : _graph[rbb]) { //for all successors of current bb
			std::cerr << *succ << " is a valid successor" << std::endl;
			edge e;
			Constraint c;
			std::tie(e,c) = rbb->getConstraintForSucc(*succ);
			std::cerr << "extracted the constraint for successor " << *succ << std::endl;
			Configuration newk{k};
			std::cerr << "Configuration copied" << std::endl;

			newk.setPredecessorInfo(rbb->getRawBB(),e->dest_idx);
			std::cerr << "Copy of configuration initialized" << std::endl;
			newk << c;
			std::cerr << "Constraint added to configuration" << std::endl;
			if (newk)
				walk.emplace(succ, newk);
			// else : abandon the path, the resulting configuration is invalid
		}
	}
}
