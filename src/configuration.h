#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <cstdlib>
#include <gcc-plugin.h>
#include <tree.h>

#include <memory>
#include <vector>
#include <map>
#include <set>
#include <utility>
#include <limits>
#include <yices.h>

struct Constraint;

struct ValueRange
{
	std::vector<term_t> _constraints;

	static bool checkVectorOfConstraints(std::vector<term_t>& constraints);
	explicit operator bool() {
		return checkVectorOfConstraints(_constraints);
	}

	void operator=(const ValueRange& other) {
	}
};

class Configuration
{
	private:
		std::map<tree,ValueRange> _varsMem;
		std::map<tree,ValueRange> _varsTemp;
		std::map<tree,std::string> _strings;

		ValueRange& getValueRangeForVar(tree var);
		const type_t Yices_int{yices_int_type()};
		const std::string& strForTree(tree t);

	public:
		explicit operator bool();
		void resetVarMem(tree varMem);
		void resetAllVarMem();
		void addConstraint(const Constraint& c);
};

Configuration& operator<<(Configuration& k, gimple stmt);
Configuration& operator<<(Configuration& k, const Constraint& c);

#endif /* ifndef CONFIGURATION_H */
