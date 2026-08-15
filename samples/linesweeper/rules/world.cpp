#include "samples/linesweeper/rules/world.h"

#include <cstring>

// The rules layer's only translation unit so far, and it includes nothing from
// engine/ - which is the property the whole layer exists to have. tick.cpp
// lands beside it.
namespace linesweeper
{
	bool identical(const World& left, const World& right)
	{
		// Legal because World has no padding bits, which world.h asserts three
		// lines above this function's declaration. Without that, the padding
		// between members is indeterminate and two matches that are the same
		// match can compare different for reasons no test could explain.
		return std::memcmp(&left, &right, sizeof(World)) == 0;
	}
}
