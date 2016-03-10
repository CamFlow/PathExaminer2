#ifndef CONFIGURATION_H
#define CONFIGURATION_H

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
	std::vector<Constraint> _constraints;
	std::vector<term_t> _terms;

	void addConstraint(const Constraint& c);

	static bool checkVectorOfConstraints(std::vector<term_t>& terms);
	explicit operator bool();
};

class Configuration
{
	private:
		std::map<tree,ValueRange> _varsMem;
		std::map<tree,ValueRange> _varsTemp;
		static std::map<tree,std::string> _strings;
		std::map<tree,tree> _ptrDestination;
		unsigned int _indexLastEdgeTaken;
		basic_block _lastBB;

		ValueRange& getValueRangeForVar(tree var);
		const type_t Yices_int{yices_int_type()};

		void doGimpleAssign(gimple stmt);
		void doGimplePhi(gimple stmt);
		void doGimpleCall(gimple stmt);

	public:
		static const std::string& strForTree(tree t);
		explicit operator bool();
		void resetVar(tree var);
		void resetAllVarMem();
		void addConstraint(const Constraint& c);
		void setPredecessorInfo(basic_block bb, unsigned int edgeTaken)
		{ 
			_lastBB = bb;
			_indexLastEdgeTaken = edgeTaken;
		}

		Configuration& operator<<(gimple stmt);
		Configuration& operator<<(const Constraint& c);
};


#endif /* ifndef CONFIGURATION_H */
