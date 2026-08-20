#pragma once

#include <vector>

// The pixel contract's third statement, and the only one that can be held
// across backends.
//
// pixel_tests.cpp asserts RELATIONSHIPS - where the ink landed, how far the
// next character is from it, that the box measure_text promised is the box
// that was filled - and says why in its own header: a glyph's edges are
// anti-aliased and its atlas is block-compressed, so the coverage value at any
// one pixel is a fact about the compressor rather than about the engine.
// Relationships survive a different rasteriser, which is exactly what makes
// them portable and exactly what makes them blind. Every term the three
// rasterising backends hand-copied from each other - the pixels-to-clip
// constant, the index loop, the HLSL at two profiles and its GLSL
// transliteration, draw_text's camera prologue - is a term every copy can get
// wrong in the same direction while every relationship in that file still
// holds.
//
// The only thing that sees a drift like that is the frame itself. So every
// frame the harness reads back is compared, byte for byte, against a checked-in
// image of what that frame is supposed to contain - and because a backend is
// chosen at configure time (T5), "d3d11 agrees with gl" is not a statement any
// one process can make. The checked-in set is what carries it between them:
// three runs, three configurations, one set of images, and a difference is a
// file somebody has to look at rather than an argument somebody has to make.
namespace golden
{
	// Compare the frame the harness just read back against the checked-in
	// image for the doctest case that is running, or write that image when the
	// run is a regeneration (LABRADOR_GOLDEN_DUMP).
	//
	// Called from Harness::end() rather than from each case, which is what
	// makes this coverage grow with the file instead of with a list somebody
	// has to remember to extend. A case added tomorrow is golden tomorrow.
	//
	// rgba is tightly packed, width * height * 4 bytes, as read_back_buffer
	// hands it over.
	void check_frame(int width, int height,
		const std::vector<unsigned char>& rgba);
}
