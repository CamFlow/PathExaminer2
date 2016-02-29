#include <vector>
#include <string>

#include <functional>

#include "rich_basic_block.h"

class Path {
private:
	std::vector<std::reference_wrapper<RichBasicBlock>> _pieces;

public:
	std::string getConstraint();
};
