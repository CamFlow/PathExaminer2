/**
 * @file evaluator.cpp
 * @brief Implementation of the Evaluator class
 * @author Laurent Georget, Michael Han
 * @version 0.2
 * @date 2019-11-06
 */
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
#include "loop_header_basic_block.h"

#include "debug.h"

Evaluator::Evaluator()
{
	yices_init();
	compute_may_aliases(); //needed for the points-to oracle

	debug() << "Building the rich basic blocks" << std::endl;
	basic_block bb;
	FOR_ALL_BB(bb) {
		debug() << "About to build basic block " << bb->index << std::endl;
		debug() << "is it a loop? " << std::boolalpha
			  << bool(bb_loop_depth(bb) > 0) << std::endl;

		RichBasicBlock* rbb =
			(bb_loop_depth(bb) > 0 && bb->loop_father->header == bb) ?
			buildLoopHeader(bb) :
			_allbbs.emplace(bb, std::unique_ptr<RichBasicBlock>(new RichBasicBlock(bb)))
				.first->second.get();

		if (rbb->hasFlowNode() && !rbb->hasLSMNode())
			_bbsWithFlows.insert(rbb);
	}
}

Evaluator::~Evaluator()
{
	yices_exit();
}

void Evaluator::evaluateAllPaths()
{
	/* We do not walk the graph from flow nodes. */
	/*
	debug() << "There are " << _bbsWithFlows.size()
		  << " bbs with flow nodes (excluding those having LSM nodes)" << std::endl;
	for (RichBasicBlock* flowBB : _bbsWithFlows) {
		debug() << "Examining " << *flowBB << std::endl;
		_graph.clear();
		buildSubGraph(flowBB);
		debug() << "These are all the basic blocks:" << std::endl;
		for (const auto& p : _allbbs) {
			debug() << *(p.second) << std::endl;
		}
		debug() << "These are the basic blocks from the interesting "
			     "subgraph:" << std::endl;
		for (const auto& p : _graph) {
			debug() << '['
				  << p.first->getRawBB()->index
				  << "] (succs in graph: ";
			for (const auto& s : p.second)
				debug() << '['
					  << s->getRawBB()->index
					  << "] ";
			debug() << ")" << std::endl;
		}
		walkGraph(flowBB);
	}
	*/
	debug() << "These are all the basic blocks:" << std::endl;
	for (const auto& p : _allbbs) {
		debug() << *(p.second) << std::endl;
	}
	edgeContraction();
	
}

LoopHeaderBasicBlock* Evaluator::buildLoopHeader(basic_block bb)
{
	assert(bb_loop_depth(bb) > 0 && bb->loop_father->header == bb);

	debug() << "Building a loop header pseudo-basic block" << std::endl;
	LoopHeaderBasicBlock* lbb = static_cast<LoopHeaderBasicBlock*>(
		_allbbs.emplace(
			bb,
			std::unique_ptr<RichBasicBlock>(
				new LoopHeaderBasicBlock(bb))
		).first->second.get());

	debug() << "Loop added for basic_block " << bb->index << std::endl;
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
		debug() << "basic block " << pred->index
		        << " is a predecessor" << std::endl;
		// if we are in a loop, we must jump to the header
		// to fetch the LoopBasicBlock
		if (bb_loop_depth(bb->getRawBB()) > 0) {
			if (pred == bb->getRawBB()->loop_father->latch) {
				//hmm pred is a basic block inside the loop
				//headed by bb
				//probably because we've just gone through the
				//latch, we can cut the current branch of
				//exploration here and explore the other preds
				debug() << "Stuck in a loop, abandoning this "
					     "path"
					  << std::endl;
				continue;
			}
		}

		//better to just crash at this point if p is NOT in the map
		RichBasicBlock* p = _allbbs.at(pred).get();

		debug() << "got the corresponding RichBasicBlock" << std::endl;

		// if the node we just discovered is new, then we must explore
		// it first before deciding the state of the current node
		if (colors[p] == Color::WHITE) {
			debug() << *p << " is white, visiting it first" << std::endl;
			dfs_visit(p, colors);
		}
		// if we point to a GREEN node (e.g. the root, then we are
		// ourselves a GREEN node)
		if (colors[p] == Color::GREEN ||
		    colors[p] == Color::GRAY) {
			debug() << *p << " is green (or gray), adding myself as a successor" << std::endl;
			auto succs = _graph.find(p);
			if (succs == _graph.cend())
				succs = _graph.emplace(p,std::vector<RichBasicBlock*>()).first;
			succs->second.push_back(bb);
			at_least_one_pred_green = true;
		}

		debug() << p << " is " << ((colors[p] == Color::GREEN) ? "green" : "red") << std::endl;
		if (at_least_one_pred_green) {
			colors[bb] = Color::GREEN;
		} else {
			colors[bb] = Color::RED;
		}
	}

}

void Evaluator::walkGraph(RichBasicBlock* dest)
{
	unsigned int pathsFound = 0;
	unsigned int pathsRejected = 0;
	debug() << "\nStarting the walk until " << *dest << std::endl;
	std::stack<std::pair<RichBasicBlock*,Configuration>> walk;
	walk.emplace(_allbbs.at(ENTRY_BLOCK_PTR).get(),Configuration());
	while (!walk.empty()) {
		RichBasicBlock* rbb = walk.top().first;
		debug() << "Reached " << *rbb << std::endl;
		Configuration k = std::move(walk.top().second);
		walk.pop();

		if (rbb == dest) {
			k.setPredecessorInfo(rbb, 0);
			std::cerr << "Found a path\n\t";
			pathsFound++;
			k.printPath(std::cerr);
			std::cerr << "\n";
			continue; //we can explore other branches
		}

		rbb->applyAllConstraints(k);
		debug() << "Handled all statements" << std::endl;

		for (const auto& succ : _graph[rbb]) { //for all successors of current bb
			debug() << *succ << " is a valid successor" << std::endl;
			edge e;
			Constraint c;
			std::tie(e,c) = rbb->getConstraintForSucc(*succ);
			debug() << "extracted the constraint for successor " << *succ << std::endl;
			Configuration newk{k};
			debug() << "Configuration copied" << std::endl;

			newk.setPredecessorInfo(rbb,e->dest_idx);
			debug() << "Copy of configuration initialized" << std::endl;
			newk << c;
			debug() << "Constraint added to configuration" << std::endl;
			if (newk)
				walk.emplace(succ, newk);
			 else //abandon the path, the resulting configuration is invalid
				pathsRejected++;

		}
	}
	std::cerr << "----------------------\n"
		  << "Result of the analysis\n"
		  << "paths found: " << pathsFound << "\n"
		  << "paths rejected: " << pathsRejected << "\n"
		  << "----------------------\n"
		  << std::endl;
}

void Evaluator::edgeContraction()
{
	/* Color all the RichBasicBlocks:
	 * GREEN: entry and exit block
	 * RED: LSM/Func blocks
	 * WHITE: Others */
	std::map<RichBasicBlock*,Color> colors;

        for (const auto& rbb : _allbbs)
                if (rbb.first == ENTRY_BLOCK_PTR ||
		    rbb.first == EXIT_BLOCK_PTR)
                        colors[rbb.second.get()] = Color::GREEN;
                else if (rbb.second->hasLSMNode() || rbb.second->hasFuncNode())
                        colors[rbb.second.get()] = Color::RED;
                else
                        colors[rbb.second.get()] = Color::WHITE;

	/* If at least one edge is contracted in the for loop. 
	 * If so, we will go through the graph again looking
	 * for more possible edge contractions. Otherwise, we
	 * will stop because there is nothing more we can do. */
	bool edge_contracted = false;
	do {
		edge_contracted = false;
		for (auto rbb = _allbbs.cbegin(); rbb != _allbbs.cend(); rbb++) {
			debug() << "Checking color for block: " << rbb->second.get()->getRawBB()->index << std::endl;
			/* The contracted model maintains entry and exit nodes. */
			if (rbb->first == ENTRY_BLOCK_PTR ||
                    	    rbb->first == EXIT_BLOCK_PTR)
				continue;
			/* The contracted model maintains LSM blocks. */
			else if (colors[rbb->second.get()] == Color::RED)
				continue;
			/* GREY blocks have already been merged away. */
			else if (colors[rbb->second.get()] == Color::GRAY)
				continue;
			else {
				debug() << "Current white block: " << rbb->second.get()->getRawBB()->index << std::endl;
				printPreds(rbb->second.get());
				printSuccs(rbb->second.get());
				for (auto sbb = rbb->second.get()->getSuccs().cbegin(); 
					  sbb != rbb->second.get()->getSuccs().cend(); sbb++) {
					/* succ is current block's succ. */
					RichBasicBlock* succ = _allbbs.at(sbb->first).get();
					/* Only edges between WHITE blocks can be contracted. */
					if (colors[succ] != Color::WHITE)
						continue;
					/* Our algorithm will create self loop. Do not merge with self. */
					if (rbb->first->index == succ->getRawBB()->index)
						continue;
					else {
						debug() << "Merging current block with: " << succ->getRawBB()->index << std::endl;
						printPreds(succ);
						printSuccs(succ);
						/* Add all current block's succ's succs to the block's succs. 
						 * Update all current block's succ's succs's pred. */
						for (auto ssbb = succ->getSuccs().cbegin();
							  ssbb != succ->getSuccs().cend(); ssbb++) {
							/* ssucc is current block's succ's succ. */
							RichBasicBlock* ssucc = _allbbs.at(ssbb->first).get();
							rbb->second.get()->getSuccs().emplace(ssucc->getRawBB(), 
											      succ->getConstraintForSucc(*ssucc));

							ssucc->getPreds().emplace(rbb->second.get()->getRawBB(),
										  succ->getConstraintForPred(*rbb->second.get()));
							ssucc->getPreds().erase(succ->getRawBB());
						}
						/* Add all current block's succ's preds (except the current block) to the block's preds. 
						 * Update current block's succ's pred's succ. */
						for (auto spbb = succ->getPreds().cbegin();
							  spbb != succ->getPreds().cend(); spbb++) {
							/* Make sure we are not adding the current block to its own preds
							 * if current block's succ's original preds include current block. */
							if (spbb->first->index == rbb->first->index)
								continue; 
							else {
								/* spred is current block's succ's pred. */
								RichBasicBlock* spred = _allbbs.at(spbb->first).get();
								rbb->second.get()->getPreds().emplace(spred->getRawBB(),
												      succ->getConstraintForPred(*spred));
								spred->getSuccs().emplace(rbb->second.get()->getRawBB(),
											  succ->getConstraintForPred(*rbb->second.get()));
								spred->getSuccs().erase(succ->getRawBB());	  
							}
						}
						/* Remove current block's succ from current block. */
                                                rbb->second.get()->getSuccs().erase(succ->getRawBB());

						debug() << "After merge, current block has: " << std::endl;
						printPreds(rbb->second.get());
                                		printSuccs(rbb->second.get());
						/* The block's succ block is now merged away logically. */
						colors[succ] = Color::GRAY;
						/* To avoid map iterator issues, let's get out of the loop and rerun. 
						 * We work on one block's succ at a time. */
						edge_contracted = true;
						break;

					}
				}
				if (edge_contracted)
					break;
                        }
                }

	} while(edge_contracted);

	printModel(colors);

}

void Evaluator::printModel(std::map<RichBasicBlock*,Color> colors)
{
	for (const auto& rbb : _allbbs) {
		RichBasicBlock* bb = rbb.second.get();
		debug() << "[ "
			<< rbb.first->index;
		if (colors[bb] == Color::WHITE) {
			debug() << " (WHITE) ";
		} else if (colors[bb] == Color::RED) {
			debug() << " (RED) ";
		} else if (colors[bb] == Color::GRAY) {
			debug() << " (GRAY) ";
		} else if (colors[bb] == Color::GREEN) {
			debug() << " (GREEN) ";
		}
		debug() << "] -> [ ";
		for (const auto& sbb : bb->getSuccs()) {
			basic_block succbb = sbb.first;
			RichBasicBlock* succrbb = _allbbs.at(succbb).get();
			if (colors[succrbb] != Color::GRAY) {
				debug() << succbb->index << " ";
			}
		}
		debug() << "]" << std::endl;
	}
}

void Evaluator::printSuccs(RichBasicBlock* rbb)
{
	debug() << "( succs: ";
	for (const auto& succ : rbb->getSuccs()) {
		debug() << succ.first->index << " ";
	}
	debug() << ")" << std::endl;
}

void Evaluator::printPreds(RichBasicBlock* rbb)
{
        debug() << "( preds: ";
        for (const auto& pred : rbb->getPreds()) {
                debug() << pred.first->index << " ";
        }
        debug() << ")" << std::endl;
}
