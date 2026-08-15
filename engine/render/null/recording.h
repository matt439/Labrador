#pragma once

#include "engine/render/renderer.h"
#include "engine/render/sprite_vertex.h"
#include "engine/render/viewport.h"

#include <vector>

// What the null backend was asked to draw.
//
// THIS HEADER IS MEANT TO BE INCLUDED FROM OUTSIDE ITS FOLDER, which is the
// opposite of the rule about engine/render/<backend>/backend.h - so the
// distinction is worth stating rather than leaving somebody to wonder whether
// this is a hole in it.
//
// The rule exists because a backend header names backend types, and one line of
// #include carries them further than a translation unit can: that is how
// <d3d11_1.h> came to be on the command line of every state file in every
// client. `backend.h` names those types and may not be included from anywhere
// else; cmake/check_engine_includes.cmake fails the build for it by name.
//
// Nothing here names a backend type. A RecordedSprite is a handle, a viewport,
// a filter and four vertices - every one of them an engine type that
// engine/render/ already hands out. There is nothing to leak, and a backend
// whose whole purpose is to be read by a test that has no device would be
// useless if the test could not read it.
//
// WHAT IT IS FOR. Three places asked for this backend by name before it
// existed: bench/scene_bench.cpp, which duplicates the render cull because
// Scene::draw needs a Renderer; tests/ui/stub_widget.h, which exists because a
// real UiText cannot be constructed without one; and .github/workflows/ci.yml,
// where a runner with no GPU cannot check the OpenGL backend's pixels. This
// answers the first two completely and the third not at all - see below.
//
// IT RECORDS WHAT WAS DRAWN, NOT WHAT IT LOOKED LIKE, and that is a deliberate
// floor rather than a first version. Rasterising would mean a third
// implementation of the pixel contract, a BC2 and BC3 decoder to sample
// textures with, and a fill rule that had to agree with two hardware ones to
// the pixel - which is a large thing to build and a larger one to be wrong
// about quietly. What a test can assert here is that an object emitted the
// quads it should have, from the texture it should have, at the positions it
// should have. Renderer::read_back_buffer therefore throws, and
// RenderPixelTests is excluded from this configuration by name.

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
	// Valid until the next begin_frame. Filled by submit(), so a caller reads
	// it in the same place RenderPixelTests reads pixels: between submit() and
	// end_frame().
	const std::vector<RecordedSprite>& recorded_sprites(
		const Renderer& renderer);
}
