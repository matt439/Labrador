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
// client.
//
// THE CHECK GUARDS THE FOLDER, NOT ONE FILENAME IN IT, and this file is inside
// that folder - so it is inside the wall, not outside it.
// cmake/check_engine_includes.cmake stopped matching `backend.h` by name
// exactly because device_resources.h was the way around that, and it now fails
// the build for any file outside engine/render/<backend>/ that names any header
// in it. What keeps this legal is narrower than it looks: the check scans
// engine/, and every includer outside the folder is a test or a benchmark -
// tests/render/null_tests.cpp, tests/scene/fanout_tests.cpp and
// bench/fanout_bench_null.cpp, each of them compiled in this configuration
// alone. Anything in engine/ that reached for this would fail the build, and
// correctly - a recording is for a test to read.
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
// where a runner with no GPU cannot check the OpenGL backend's pixels. It
// UNBLOCKS the first two and answers the third not at all. Neither of the first
// two has actually been converted: scene_bench.cpp:144-154 still duplicates the
// cull and says why, and stub_widget.h:14-21 still stubs and says "that is
// worth doing and is not done". Both call sites were amended to record that
// they were not converted; this paragraph claimed they had been, which is the
// drift that matters - a header saying a job is finished is the one place
// nobody looks to find out that it is not.
//
// THE FIRST OF THE THREE IS NOW HALF ANSWERED, AND THE HALF MATTERS. Scene::draw
// is driven for real in bench/fanout_bench_null.cpp and pinned in
// tests/scene/fanout_tests.cpp, which is what the paragraph above was reaching
// for - but scene_bench.cpp's duplicated cull stays exactly where it is, for
// the reason it gives rather than for want of this header: that benchmark
// builds in all five configurations and a case that only runs in one would
// measure a different thing depending on the preset. The conversion went into
// a file of its own instead. So the sentence to keep is the narrow one: this
// backend unblocked driving Scene::draw without a device, and doing so did not
// remove one line from the file that could not.
//
// IT RECORDS WHAT WAS DRAWN, NOT WHAT IT LOOKED LIKE, and that is a deliberate
// floor rather than a first version. Rasterising would mean a third
// implementation of the pixel contract, a BC2 and BC3 decoder to sample
// textures with, and a fill rule that had to agree with two hardware ones to
// the pixel - which is a large thing to build and a larger one to be wrong
// about quietly. What a test can assert here is that an object emitted the
// quads it should have, from the texture it should have, at the positions it
// should have. Renderer::read_back_buffer therefore throws std::logic_error,
// and RenderPixelTests is NOT BUILT AT ALL in this configuration rather than
// built and excluded - tests/render/CMakeLists.txt says why, and a test that
// cannot pass is better absent than skipped.

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
