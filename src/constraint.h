#ifndef CONSTRAINT_H
#define CONSTRAINT_H value

#include <gcc-plugin.h>
#include <basic-block.h>
#include <tree.h>
#include <string>

struct Constraint {
	std::string lhs;
	tree_code rel = MAX_TREE_CODES;
	std::string rhs;

	Constraint() = default;
	Constraint(edge e);
};

#endif /* ifndef CONSTRAINT_H */
