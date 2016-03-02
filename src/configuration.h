#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <cstdlib>
#include <gcc-plugin.h>
#include <tree.h>

#include <map>
#include <set>
#include <utility>
#include <limits>

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
		std::map<tree,ValueRange> _values;

	public:
		explicit operator bool() const {
			bool ok = true;
			for (auto&& it = _values.cbegin();
			     it != _values.cend() && ok;
			     ++it) {
				const ValueRange& range = it->second;
				if (range)
					ok = false;
			}
			return ok;
		}
};

#endif /* ifndef CONFIGURATION_H */
