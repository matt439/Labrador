#include "tests/render/d3d12/wait_observer.h"

// Include every production dependency before introducing the one-call macro,
// so SDK declarations and standard-library headers are never rewritten.
#include "engine/render/d3d12/device_resources.h"
#include "engine/render/throw_if_failed.h"

#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <tuple>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

// This executable supplies the entire object, leaving the original static
// archive member unextracted. Never link it with /WHOLEARCHIVE or suppress
// duplicate definitions. The class layout and all other code stay identical.
#define WaitForSingleObjectEx ::labrador::d3d12_wait_test::bounded_wait
#include "engine/render/d3d12/device_resources.cpp"
#undef WaitForSingleObjectEx
