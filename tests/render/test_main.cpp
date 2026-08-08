#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

// The renderer seam's contract, compiled so it cannot rot before B1 fills it
// in. Nothing includes engine/render/renderer.h yet and headers are not build
// targets, so without this line a typo in the file that specifies the largest
// change on the plan would go unnoticed until somebody tried to implement it.
// Delete this include when a real test in this folder needs the header.
#include "engine/render/renderer.h"
