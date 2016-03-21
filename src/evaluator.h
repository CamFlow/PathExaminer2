#ifndef EVALUATOR_H
#define EVALUATOR_H

#include <vector>
#include <functional>
#include <map>
#include <set>
#include <utility>
#include <memory>

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

		LoopBasicBlock* buildLoop(basic_block bb);

		void buildSubGraph(RichBasicBlock* start);
		void walkGraph(RichBasicBlock* destination);
		void dfs_visit(RichBasicBlock* bb, std::map<RichBasicBlock*,Color>& colors);
		std::map<basic_block,std::unique_ptr<RichBasicBlock>> _allbbs;
		std::set<RichBasicBlock*> _bbsWithFlows;
		std::map<RichBasicBlock*,std::vector<RichBasicBlock*>> _graph;
};

#endif /* ifndef EVALUATOR_H */
