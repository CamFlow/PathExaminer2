#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <cstdlib>
#include <gcc-plugin.h>
#include <tree.h>

#include <map>
#include <set>
#include <utility>
#include <limits>

struct Constraint;

struct ValueRange
{
	explicit operator bool() const {
		return false;
	}

	void operator=(const ValueRange& other) {
	}
};


class Configuration
{
	private:
		std::map<tree,ValueRange> _varsMem;
		std::map<tree,ValueRange> _varsTemp;

	public:
		explicit operator bool() const {
			bool ok = true;
			for (auto&& it = _varsMem.cbegin();
			     it != _varsMem.cend() && ok;
			     ++it) {
				const ValueRange& range = it->second;
				if (range)
					ok = false;
			}
			for (auto&& it = _varsTemp.cbegin();
			     it != _varsTemp.cend() && ok;
			     ++it) {
				const ValueRange& range = it->second;
				if (range)
					ok = false;
			}
			return ok;
		}
};

Configuration& operator<<(Configuration& k, gimple stmt);
Configuration& operator<<(Configuration& k, const Constraint& c);

#endif /* ifndef CONFIGURATION_H */
