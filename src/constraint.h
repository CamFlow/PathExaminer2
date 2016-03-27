/**
 * @file constraint.h
 * @brief Definition of the Constraint class
 * @author Laurent Georget
 * @version 0.1
 * @date 2016-03-27
 */
#ifndef CONSTRAINT_H
#define CONSTRAINT_H

#include <gcc-plugin.h>
#include <basic-block.h>
#include <tree.h>
#include <string>

/**
 * @brief Describes a simple numeric condition that must be satisfied on a
 * variable
 */
struct Constraint {
	/**
	 * @brief the left-hand side of this constraint, a variable
	 */
	tree lhs;
	/**
	 * @brief the relational operator between the left-hand side and the
	 * right-hand side
	 *
	 * The accepted values are:
	 * <ul>
	 * <li><code>EQ_EXPR</code></li>
	 * <li><code>NE_EXPR</code></li>
	 * <li><code>LT_EXPR</code></li>
	 * <li><code>LE_EXPR</code></li>
	 * <li><code>GT_EXPR</code></li>
	 * <li><code>GE_EXPR</code></li>
	 * </ul>
	 */
	tree_code rel = MAX_TREE_CODES;
	/**
	 * @brief the right-hand side of this constraint, a variable or an
	 * integer value or an adress-of
	 */
	tree rhs;

	/**
	 * @brief Builds an empty, invalid constraint
	 */
	Constraint() = default;
	/**
	 * @brief Builds a constraint from an edge, to represent its guard
	 * @param e the edge from which the constraint is built
	 */
	explicit Constraint(edge e);
	/**
	 * @brief Builds a constraint explicitly from its three components
	 * @param lhs the left-hand side
	 * @param rel the relational operator
	 * @param rhs the right-hand side
	 */
	Constraint(tree lhs, tree_code rel, tree rhs);
	/**
	 * @brief Compares two Constraint lexicographically
	 *
	 * This operator is provided for containers like std::map or std::set.
	 * @param c1 one constraint
	 * @param c2 another
	 * @return true if, and only if, \a c1 = (lhs1,rel1,rhs1) is
	 * lexicographically lesser than \a c2 = (lhs2,rel2,rhs2)
	 */
	friend bool operator<(const Constraint& c1, const Constraint& c2);
};

#endif /* ifndef CONSTRAINT_H */
