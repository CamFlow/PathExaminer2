#ifndef EVALUATOR_H
#define EVALUATOR_H

#include <vector>
#include <functional>
#include <map>
#include <utility>

#include "path.h"
#include "rich_basic_block.h"

class Configuration;
struct Constraint;
class LoopBasicBlock;

class Evaluator {
	public:
		Evaluator();
		void evaluateAllPaths();

	private:
		enum class Color {
			WHITE, GRAY, GREEN, RED
		};
		class RichBasicBlockLess {
		public:
			bool operator()(const RichBasicBlock& lhs, const RichBasicBlock& rhs)
			{
				return lhs.getRawBB() < rhs.getRawBB();
			}
		};

		LoopBasicBlock& buildLoop(basic_block bb);

		void buildSubGraph(RichBasicBlock& start);
		void walkGraph(RichBasicBlock& destination);
		void dfs_visit(RichBasicBlock& bb, std::map<std::reference_wrapper<RichBasicBlock>,Color,RichBasicBlockLess>& colors);
		std::map<basic_block,RichBasicBlock> _allbbs;
		std::vector<std::reference_wrapper<RichBasicBlock>> _bbsWithFlows;
		std::map<std::reference_wrapper<RichBasicBlock>,std::vector<RichBasicBlock>,RichBasicBlockLess> _graph;
};

#endif /* ifndef EVALUATOR_H */
