/**
 * @file evaluator.h
 * @brief Definition of the Evaluator class
 * @author Laurent Georget, Michael Han
 * @version 0.2
 * @date 2019-11-06
 */
#ifndef EVALUATOR_H
#define EVALUATOR_H

#include <vector>
#include <functional>
#include <map>
#include <set>
#include <utility>
#include <memory>

#include "rich_basic_block.h"

class Configuration;
struct Constraint;
class LoopHeaderBasicBlock;

/**
 * @brief Main class, responsible for computing all interesting execution paths
 * and tell whether they are possible
 */
class Evaluator {
	public:
		/**
		 * @brief Builds an Evaluator and initializes it
		 *
		 * Initializing an Evaluator requires the following operations:
		 * <ul>
		 * <li>initializing Yices, the SMT solver</li>
		 * <li>computing the aliasing information (a call to a GCC API)</li>
		 * <li>building all the rich basic blocks of the current function</li>
		 * </ul>
		 */
		Evaluator();
		/**
		 * @brief Frees the SMT solver resources
		 */
		~Evaluator();
		/**
		 * @brief Runs the evaluation on the function
		 *
		 * Construct a model for the function that contains only:
		 * <ul>
		 * <li>GREEN entry and exit basic blocks</li>
		 * <li>RED basic blocks that contain LSM nodes or function calls</li>
		 * <li>WHITE basic blocks as connectors</li>
		 * </ul>
		 */
		void evaluateAllPaths();

	private:
		/**
		 * @brief The colors the basic blocks can be assigned during
		 * the graph construction
		 */
		enum class Color {
			WHITE, //! White basic blocks are basic blocks yet to be explored or used as connecting blocks
			GRAY, //! Gray basic blocks are basic blocks already seen and merged
			GREEN, //! Green basic blocks are entry and exit basic blocks
			RED //! Red basic blocks are basic blocks which contain LSM hooks or functon calls
		};
		
		/**
		 * @brief Contract edges and merge nodes to build a LSM hook model
		 */
		void edgeContraction();

		/**
		 * @brief Builds a rich basic block representing a loop header
		 *
		 * The pointer which is returned must be freed at the
		 * destruction of the Evaluator.
		 * @param bb any basic block belonging to the loop
		 * @return a pointer to a newly allocated LoopBasicBlockHeader
		 */
		LoopHeaderBasicBlock* buildLoopHeader(basic_block bb);

		/**
		 * @brief Builds the minimal basic block subgraph necessary for the exploration of all paths from the root to a given basic block
		 *
		 * The basic block subgraph is the graph where :
		 * <ul>
		 * <li>a node is a rich basic block</li>
		 * <li>an edge exists from one node to another if it exists in the CFG and if it is part of a path from the root basic block to the target basic block which do not go through a basic block containing a LSM hook</li>
		 * </ul>
		 * @param start the target basic block
		 */
		void buildSubGraph(RichBasicBlock* start);
		/**
		 * @brief Visits a node in the subgraph and updates its colors
		 * as well as its predecessors'
		 *
		 * This function is recursive. It implements a depth-first
		 * visit, with all edges reversed.
		 * @param bb the basic block to visit
		 * @param colors the map of colors built so far
		 */
		void dfs_visit(RichBasicBlock* bb, std::map<RichBasicBlock*,Color>& colors);
		/**
		 * @brief Walks a fully built subgraph in order to decide
		 * whether, in this graph, the paths that go from the root
		 * to a target basic block are possible
		 *
		 * This implements a depth-first visit too, but this time with
		 * the edges in the correct direction.
		 * @param destination the target basic block
		 */
		void walkGraph(RichBasicBlock* destination);

		/**
		 * @brief The data structre where all rich basic blocks are
		 * stored, with the correspondence with the GCC basic blocks
		 */
		std::map<basic_block,std::unique_ptr<RichBasicBlock>> _allbbs;
		/**
		 * @brief The set of basic blocks containing a flow instruction
		 *
		 * For each basic block in this set, the paths must be analyzed.
		 */
		std::set<RichBasicBlock*> _bbsWithFlows;
		/**
		 * @brief The subgraph of rich basic blocks the walkGraph method visits
		 */
		std::map<RichBasicBlock*,std::vector<RichBasicBlock*>> _graph;
		/**
		 * @brief Print the final LSM hook model in JSON format.
		 */
		void jsonModel(std::map<RichBasicBlock*,Color> colors);
		/**
		 * @brief Print the final LSM hook model of the function.
		 */
		void printModel(std::map<RichBasicBlock*,Color> colors);
		/**
		 * @brief Print the predecessors of a basic block in the  CFG.
		 */
		void printPreds(RichBasicBlock* rbb);
		/**
		 * @brief Print the successors of a basic block in the CFG.
		 */
		void printSuccs(RichBasicBlock* rbb);
};

#endif /* ifndef EVALUATOR_H */
