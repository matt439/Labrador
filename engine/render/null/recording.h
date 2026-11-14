#pragma once

#include "engine/render/renderer.h"
#include "engine/render/sprite_vertex.h"
#include "engine/render/viewport.h"

#include <vector>

// The null backend's submitted sprites, for headless tests and benchmarks.
//
// Tests and benchmarks built with null render may include this header.
// cmake/check_engine_includes.cmake scans engine/ and forbids includes from
// outside engine/render/null/; this recording interface grants no exception
// to engine code. Its fields use the render seam's handles and geometry.
//
// A recording exposes sprite order, textures, filtering, viewports and corners.
// It produces no pixels and cannot establish rasterization or visual agreement
// with another backend. Renderer::read_back_buffer throws std::logic_error,
// and tests/render/CMakeLists.txt omits RenderPixelTests for null render.

namespace labrador
{
	// One sprite, exactly as a real backend would have received it: the corners
	// are in view pixels with the camera already applied, which is what
	// engine/render/sprite_geometry.h produces and what every backend uploads.
	struct RecordedSprite
	{
		int view = 0;
		TextureHandle texture;
		TextureFilter filter = TextureFilter::point;
		Viewport viewport;
		SpriteVertex corners[4] = {};
	};

	// Everything drawn since the last begin_frame, in view order and then in
	// call order - which is the order the seam guarantees and the only one it
	// guarantees.
	//
	// Filled by submit() and cleared by begin_frame(). Read after submit(),
	// before the next begin_frame(); end_frame() leaves the recording intact.
	// The renderer owns the vector. Copy entries needed beyond its lifetime or
	// the next frame.
	const std::vector<RecordedSprite>& recorded_sprites(
		const Renderer& renderer);

	// One post pass, as a device backend draws it: which material - identity
	// for a CRT whose values turn every term off (crt_values_are_neutral) -
	// with the values set for the chain's effect, at what size, and how many
	// views it drew over: the processed views, which precede it in submit().
	// Views from processed_views on are overlay views, drawn after it.
	struct RecordedPass
	{
		PostEffect effect = PostEffect::identity;
		PostValues values;
		int processed_views = 0;
		int width = 0;
		int height = 0;
	};

	// The chain's passes this frame, in order, under the same validity rules
	// as recorded_sprites: filled by submit(), cleared by begin_frame(). Empty
	// when no chain is set.
	const std::vector<RecordedPass>& recorded_passes(const Renderer& renderer);
}
