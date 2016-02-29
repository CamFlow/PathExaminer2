#ifndef EVALUATOR_H
#define EVALUATOR_H value

#include <vector>
#include <functional>
#include <map>
#include <utility>

#include "path.h"

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

		void buildSubGraph(RichBasicBlock& start);
		void walkGraph();
		void dfs_visit(std::pair<const basic_block,RichBasicBlock>& bb, std::map<std::reference_wrapper<RichBasicBlock>,Color,RichBasicBlockLess> colors);
		std::map<basic_block,RichBasicBlock> _allbbs;
		std::vector<std::reference_wrapper<RichBasicBlock>> _bbsWithFlows;
		std::map<RichBasicBlock,std::vector<RichBasicBlock>,RichBasicBlockLess> _graph;
};

#endif /* ifndef EVALUATOR_H */