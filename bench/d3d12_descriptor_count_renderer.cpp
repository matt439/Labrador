// Preload renderer.cpp's declaration headers. The macros below instrument
// only its current standalone call statements, never declarations or SDK code.
#include "bench/d3d12_descriptor_count_state.h"
#include "engine/render/d3d12/backend.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/sprite_geometry.h"
#include "engine/render/sprite_vertex.h"
#include "engine/render/throw_if_failed.h"
#include <cstddef>
#include <cstring>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

// There are two GPU heap-start assignments (sampler/texture), two RTV getter
// assignments, one frame wait and one present in this included source. These
// append statements after successful calls; expression nesting is unsupported.
// FXC's unguarded byte-array headers are included only once, by renderer.cpp;
// they contain no declarations using these method names.
#define GetGPUDescriptorHandleForHeapStart() GetGPUDescriptorHandleForHeapStart(); descriptor_query_counts::gpu_query(__func__)
#define back_buffer_view() back_buffer_view(); descriptor_query_counts::back_buffer_query()
#define wait_for_frame() wait_for_frame(); descriptor_query_counts::begin_frame()
#define present() present(); descriptor_query_counts::end_frame()
#include "engine/render/d3d12/renderer.cpp"
#undef present
#undef wait_for_frame
#undef back_buffer_view
#undef GetGPUDescriptorHandleForHeapStart
