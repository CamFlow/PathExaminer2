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

class Configuration
{
	private:
		std::vector<std::pair<Constraint,term_t>> _constraints;
		static std::map<tree,std::string> _strings;
		std::map<tree,tree> _ptrDestination;
		unsigned int _indexLastEdgeTaken;
		basic_block _lastBB;

		const static type_t YICES_INT;

		void doGimpleAssign(gimple stmt);
		void doGimplePhi(gimple stmt);
		void doGimpleCall(gimple stmt);
		void doAddConstraint(Constraint c);

	public:
		Configuration();
		static const std::string& strForTree(tree t);
		static term_t getNormalizedTerm(tree t);
		explicit operator bool();
		void resetVar(tree var);
		void resetAllVarMem();
		bool tryAddConstraint(Constraint c);
		void setPredecessorInfo(basic_block bb, unsigned int edgeTaken)
		{
			_lastBB = bb;
			_indexLastEdgeTaken = edgeTaken;
		}
		static bool checkVectorOfConstraints(std::vector<term_t>& terms);
		Configuration& operator<<(gimple stmt);
		Configuration& operator<<(const Constraint& c);
};


#endif /* ifndef CONFIGURATION_H */
