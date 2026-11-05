// Preload every header renderer.cpp includes, so that each is first included
// before the macros below exist and none of its declarations is rewritten.
// bench/CMakeLists.txt refuses to configure this probe when renderer.cpp
// includes a header this list lacks.
#include "bench/d3d12_descriptor_count_state.h"
#include "engine/render/d3d12/backend.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/sprite_geometry.h"
#include "engine/render/sprite_vertex.h"
#include "engine/render/text_encoding.h"
#include "engine/render/throw_if_failed.h"
#include <cstddef>
#include <cstring>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

// The macros instrument only renderer.cpp's standalone call statements, never
// declarations or SDK code. There are two GPU heap-start assignments
// (sampler/texture), two RTV getter assignments, one frame wait and one
// present in the included source. These append statements after successful
// calls; expression nesting is unsupported.
#define GetGPUDescriptorHandleForHeapStart() GetGPUDescriptorHandleForHeapStart(); descriptor_query_counts::gpu_query(__func__)
#define back_buffer_view() back_buffer_view(); descriptor_query_counts::back_buffer_query()
#define wait_for_frame() wait_for_frame(); descriptor_query_counts::begin_frame()
#define present(interval) present(interval); descriptor_query_counts::end_frame()
#include "engine/render/d3d12/renderer.cpp"
#undef present
#undef wait_for_frame
#undef back_buffer_view
#undef GetGPUDescriptorHandleForHeapStart
