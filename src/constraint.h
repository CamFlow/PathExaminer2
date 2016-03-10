#ifndef CONSTRAINT_H
#define CONSTRAINT_H value

#include <gcc-plugin.h>
#include <basic-block.h>
#include <tree.h>
#include <string>

struct Constraint {
	tree lhs;
	tree_code rel = MAX_TREE_CODES;
	tree rhs;

	Constraint() = default;
	Constraint(edge e);
	Constraint(tree lhs, tree_code rel, tree rhs);
};

#endif /* ifndef CONSTRAINT_H */
