#include "tests/render/d3d12/wait_observer.h"

// Include every production dependency before introducing the one-call macro,
// so SDK declarations and standard-library headers are never rewritten.
#include "engine/render/d3d12/device_resources.h"
#include "engine/core/diagnostic.h"
#include "engine/render/device_unavailable.h"
#include "engine/render/direct3d_startup.h"
#include "engine/render/dxgi_device_kind.h"
#include "engine/render/text_encoding.h"
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
#define WaitForSingleObjectEx ::render_tests::d3d12_wait::bounded_wait
#include "engine/render/d3d12/device_resources.cpp"
#undef WaitForSingleObjectEx
