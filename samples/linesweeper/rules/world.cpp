#include "samples/linesweeper/rules/world.h"

#include <cstring>

// Like tables.cpp and tick.cpp beside it, this includes nothing from engine/ -
// which is the property the whole layer exists to have.
namespace linesweeper
{
	bool identical(const World& left, const World& right)
	{
		// Legal because World has no padding bits, which world.h's
		// has_unique_object_representations_v assert, above this function's
		// declaration, guarantees. Without that, the padding between members is
		// indeterminate and two matches that are the same match can compare
		// different for reasons no test could explain.
		return std::memcmp(&left, &right, sizeof(World)) == 0;
	}
}
