
#include <gcc-plugin.h>
#include <gimple.h>
#include <tree.h>

#include "configuration.h"
#include "constraint.h"

Configuration& operator<<(Configuration& k, gimple stmt)
{
	switch (gimple_code(stmt)) {
		default:
			; //nothing to do
	}
	return k;
}

Configuration& operator<<(Configuration& k, const Constraint& c)
{
	switch (c.rel) {
		default:
			; //we should probably raise an exception here
			  //it means that we don't handle one of the operator
			  //or that we mis-parsed the constraint
	}
	return k;
}
