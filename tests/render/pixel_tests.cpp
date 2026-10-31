#include "tests/render/pixel_test_harness.h"

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/renderer.h"

#include <doctest/doctest.h>

#include <tuple>
#include <vector>

using namespace labrador;
using namespace render_tests::pixel;

// RenderPixelTests uses this shared fixture and five contract case groups:
// pixel_tests.cpp: device, frame, resize, presentation and readback.
// sprite_pixel_tests.cpp: sprite geometry, filters, blending and call order.
// text_pixel_tests.cpp: glyph placement, text layout and transforms.
// view_pixel_tests.cpp: viewports, view isolation and scene composition.
// resource_pixel_tests.cpp: texture formats, decoding and replacement.
// pixel_test_harness.cpp owns setup/readback; golden_image.cpp owns names.

// The first test of anything this engine draws.
//
// WHY THIS CAN EXIST AT ALL. device_resources.cpp falls back to a WARP device
// in every build that is not NDEBUG, so a debug build makes a real Direct3D
// device with no GPU and no display attached - which means the whole drawing
// path is reachable from a test executable. Renderer::read_back_buffer is the
// way to see the result, and is what turns "the seam serves headless testing"
// from a claim into something a build can check.
//
// AND WHY IT DOES NOT MAKE THE NULL BACKEND UNNECESSARY. WARP is a D3D-only
// luxury: an in-box, fully conformant software rasteriser that no other API
// gets on Windows. Desktop OpenGL and Vulkan have none there - Mesa's llvmpipe
// and lavapipe are not in the box - so natively these tests run against them
// only on a machine with a working driver; the browser build draws through
// Chrome's SwiftShader instead. The null backend is still what makes the seam
// testable everywhere; this file is what tells it what the right answers are.
//
// WHAT THESE PIN. Every term below is one a backend could differ on with
// nothing else saying so - the samples would simply look slightly wrong, and
// only to somebody who knew what to look for. A backend is correct when this
// file passes against it, and each case names the term it fixes rather than
// describing what it does.
//
// WHAT THEY DO NOT PIN, said plainly rather than left to be discovered:
// non-uniform scale, flipped text, and the clamp that stops a glyph with a
// negative left bearing hanging off the start of its line - which no glyph in
// this font has, so no assertion here can reach it.
//
// Most frames are also checked against the PNG set in tests/render/golden/,
// holding every rasterising backend to the same fixed reference. Three resize
// cases opt out because their size changes or differs by backend; the
// scene-composition and off-buffer viewport cases instead assert every pixel
// against analytically known red/blue images.
// No reference is generated from the output being tested.
// Harness::end_not_comparable marks these exceptions explicitly.
//
// AND ONE MORE THAT IS NOT A GAP IN THE LIST BUT A PROPERTY OF THE METHOD.
// Because the text cases below are relationships (see two paragraphs down), a
// term added EQUALLY TO EVERY GLYPH cancels out of every one of them and this
// file cannot see it at all. The glyph's vertical bearing is exactly such a
// term: deleting it leaves this file green on every backend that rasterises,
// the null backend's record test green, and the samples merely a few pixels
// wrong.
// It is pinned in tests/render/sprite_geometry_tests.cpp instead, as a
// difference between two glyphs whose bearings differ - which is the shape an
// absolute term has to be asked about, and which needs no device. Anything
// else uniform across a line belongs there for the same reason.
//
// TEXT IS PINNED IN text_pixel_tests.cpp, and it is pinned as
// relationships rather than as colours. A sprite case can assert that one
// pixel is exactly red because the test texture has four flat texels in it. A
// glyph has neither property: its edges are anti-aliased, and the atlas a
// .spritefont carries is block-compressed, so the coverage value at any one
// pixel is a fact about the compressor. What a caller actually depends on is
// where the ink landed, how far the next character is from it, and that the
// box measure_text promised is the box that was filled - and every one of those
// is a statement about two rectangles, which survives a different rasteriser
// exactly as the sprite assertions do.
//
// THE TEXT TERMS MATTER MORE THAN THE SPRITE ONES: the pen arithmetic is a
// pile of quirks - a per-glyph left bearing, an XAdvance that is an adjustment
// to the advance rather than the advance, a whitespace glyph that steps the pen
// without drawing, a line height that is the larger of the glyph and the line
// spacing - and a rewrite that dropped any one of them would still draw legible
// text. Each case below names the term it fixes.
//
// A HIDDEN WINDOW, not an offscreen target. create_device wants a window
// handle because a swap chain does, so the cheapest honest answer is a real
// window that is never shown. It adds nothing to the seam, and if an offscreen
// path is ever wanted for its own sake it can replace this without touching a
// single assertion below.
//
// AND IN A BROWSER, A CANVAS ON A PAGE NOBODY LOOKS AT. The browser build runs
// this file in headless Chrome (tests/render/web/run_page.mjs), against the same
// golden set, so the GLES dialect gl/ compiles there is held to the same pixels
// as the desktop's. The window is behind test_window.h, and what a canvas
// costs is said in tests/render/web/test_window.cpp. Two other terms differ
// and say so where they are decided: the device-info case's present mode
// in this file, and golden_image.cpp's dumping(), which never regenerates from a
// browser. Every case below runs in one synchronous main(), which
// is what lets read_back_buffer see a frame that a canvas without
// preserveDrawingBuffer throws away the moment the page composites.

TEST_CASE("the renderer reports the rasterising device it selected")
{
	Harness harness;
	const RenderDeviceInfo& info = harness.renderer().device_info();

	CHECK_FALSE(info.backend.empty());
	CHECK_FALSE(info.api.empty());
	CHECK_FALSE(info.device_name.empty());
	CHECK(info.kind != RenderDeviceKind::null_device);
	if (info.backend == "gl")
	{
		// Two contexts, and the platform says which one this is. A browser
		// paces frames itself and has no swap interval to ask for or report
		// (gl/web_context.cpp).
#if defined(__EMSCRIPTEN__)
		CHECK(info.present_mode == "browser_animation_frame");
		CHECK_FALSE(info.requested_swap_interval.has_value());
		CHECK_FALSE(info.reported_swap_interval.has_value());
#else
		CHECK(info.present_mode == "wgl_swap_interval");
		REQUIRE(info.requested_swap_interval.has_value());
		CHECK(*info.requested_swap_interval == 1);
		REQUIRE(info.reported_swap_interval.has_value());
		CHECK(*info.reported_swap_interval == 1);
#endif
	}
	else if (info.backend == "d3d11" || info.backend == "d3d12")
	{
		CHECK(info.present_mode == "dxgi_sync_interval");
		REQUIRE(info.requested_swap_interval.has_value());
		CHECK(*info.requested_swap_interval == 1);
		CHECK_FALSE(info.reported_swap_interval.has_value());
	}
	else
	{
		CHECK(info.backend == "vulkan");
		CHECK(info.present_mode == "fifo");
		CHECK_FALSE(info.requested_swap_interval.has_value());
		CHECK_FALSE(info.reported_swap_interval.has_value());
	}
}

TEST_CASE("a frame nobody drew into is cleared to opaque black")
{
	Harness harness;

	std::ignore = harness.begin();
	harness.end();

	CHECK(harness.at(0, 0) == black_pixel);
	CHECK(harness.at(buffer_side / 2, buffer_side / 2) == black_pixel);
	CHECK(harness.at(buffer_side - 1, buffer_side - 1) == black_pixel);
}

TEST_CASE("CONTRACT: a frame that is never submitted contributes nothing")
{
	Harness harness;

	// A SPRITE AND THEN TEXT, because the texture change between them is what
	// forces a flush. Everything a draw records is CPU-side until something
	// flushes it, so a frame holding one texture's worth of sprites strands
	// nothing on any backend and would pass this on the code that fails it.
	DrawList abandoned = harness.begin();
	abandoned.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, static_cast<float>(buffer_side),
			static_cast<float>(buffer_side)),
		Colour::white, 0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
	abandoned.draw_text(harness.font, L"AAAA", Vector2F(4.0f, 4.0f),
		Colour::white, 1.0f, 0.0f, Vector2F::zero, 0.0f);

	// AND NO end(), which is the whole case. A client reaches this state by
	// letting an exception out of its draw walk and carrying on; every sample
	// exits instead, so nothing in the tree reaches it, which is why the
	// backends were free to disagree about it. See
	// docs/review/backend-equivalence/README.md, defect B.

	std::ignore = harness.begin();
	harness.end();

	// The next frame is the clear and nothing else. Not sampled - the stranded
	// geometry is a full-screen quad, but the failure it stands for is any
	// already-flushed run arriving at the head of the next frame's commands.
	CHECK(harness.at(0, 0) == black_pixel);
	CHECK(harness.ink_bounds() == RectangleI::zero);
}

TEST_CASE("CONTRACT: read_back_buffer hands back exactly back_buffer_size")
{
	Harness harness;

	// back_buffer_size is called by the harness on every frame now
	// (Harness::read_frame, added a day after this comment so that at() could
	// address the three frames whose buffer is not 64x64), but this case is
	// still the only thing in tests/ that ASSERTS on it - which is what turns
	// the seam's "exactly width * height * 4 bytes" from a sentence into
	// something a build can fail.
	std::ignore = harness.begin();
	harness.end();

	CHECK(harness.buffer_size() ==
		Vector2F(static_cast<float>(buffer_side),
			static_cast<float>(buffer_side)));
	CHECK(harness.byte_count() ==
		static_cast<size_t>(buffer_side) * buffer_side * 4);

	SUBCASE("and it still does when the window changed and nobody said so")
	{
		// THE ONE STATE IN WHICH A BACKEND CAN HOLD TWO ANSWERS TO "how big is
		// it". D3D11 draws into a swap chain it made at the size it was told,
		// so the window moving under it changes nothing here and the stretch
		// happens at Present. A WGL context has no such buffer - its default
		// framebuffer is the window's client area - so the GL backend has to
		// read the window, and answering from a cached int that only
		// create_device and window_size_changed write is wrong exactly here.
		//
		// WHAT THIS CANNOT SEE is where the pane went, and the shrink below is
		// as close as the seam gets. A stale flip height displaces the whole
		// frame in the window, but read_back_buffer is flipped against the same
		// stale number, so on a grow the two errors cancel exactly and the
		// image that comes back is correct; on a shrink the readback runs off
		// the end of the framebuffer, which GL leaves undefined rather than
		// wrong. Nothing inside this seam can assert on the window's real
		// pixels - see docs/review/backend-equivalence/TEST-GAP.md. What is
		// assertable is that the two answers agree, which is what makes the
		// size above a fact about the buffer rather than about a cache.
		harness.resize_window(buffer_side, buffer_side - 16);

		DrawList list = harness.begin();
		const Vector2F pane = harness.buffer_size();
		list.set_viewport(Viewport(0.0f, 0.0f, pane.x, pane.y));
		list.draw_sprite(harness.quad, Harness::white_texel(),
			RectangleF(0.0f, 0.0f, pane.x, pane.y), Colour::white, 0.0f,
			Vector2F::zero, SpriteFlip::none, 0.0f);
		// The one frame here that is not held against a golden image, because
		// the buffer it was read out of is a different size on each backend and
		// the seam says it must be. Harness::end_not_comparable explains.
		harness.end_not_comparable();

		const Vector2F buffer = harness.buffer_size();
		CHECK(harness.byte_count() == static_cast<size_t>(buffer.x) *
			static_cast<size_t>(buffer.y) * 4);

		// And a pane the size of that buffer covers it corner to corner, which
		// is the placement half of the same statement.
		CHECK(harness.at(0, 0) == white_pixel);
		CHECK(harness.at(static_cast<int>(buffer.x) - 1,
			static_cast<int>(buffer.y) - 1) == white_pixel);
	}
}

TEST_CASE("CONTRACT: a resize arriving mid-frame restarts the frame")
{
	// ONE OF TWO CASES HERE THAT ARE NOT ABOUT A PIXEL, and it is in this file
	// because it is the only one with a device. renderer.h makes it a term of
	// the seam that window_size_changed may arrive between begin_frame and
	// submit - a window message can be SENT rather than posted, and
	// Application::set_resolution sends one through SetWindowPos on the calling
	// thread, so a client that changes resolution from inside its own draw walk
	// gets this mid-frame in one stack. No rule forbidding it would be one the
	// caller could keep.
	//
	// IT WAS WRITTEN BECAUSE ALL THREE BACKENDS ANSWERED DIFFERENTLY AND TWO
	// WERE WRONG. OpenGL carried on; D3D11 threw DXGI_ERROR_INVALID_CALL out of
	// ResizeBuffers, because its deferred contexts still held the render
	// target; D3D12 destroyed the back buffer and then executed command lists
	// that still named it, which killed the process. Nothing in the tree said
	// what should happen, so nothing was wrong with any of them.
	Harness harness;
	Renderer& renderer = harness.renderer();

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 32.0f, 32.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

	// Here, with a draw recorded and the list still in the caller's hand.
	//
	// THE WINDOW IS RESIZED TOO, AND NOT ONLY THE RENDERER TOLD, because
	// otherwise this case would be asking one backend a different question
	// from the other three. back_buffer_size is answered from the swap chain on the
	// Direct3D backends, from the engine-owned target on Vulkan and from the
	// window on the OpenGL one - SEAM.md#6 says so, and the drag-resize case
	// above is built on the difference - so telling the renderer about a
	// resize the window never had would leave GL drawing into 64x64 while the
	// other three moved to 32x32. A real resize
	// moves both, in this order: the window changes, then the shell says so.
	harness.resize_window(buffer_side / 2, buffer_side / 2);
	CHECK(renderer.window_size_changed(buffer_side / 2, buffer_side / 2));

	// THE LIST THE CALLER IS HOLDING IS STILL A LIST. That is the half of the
	// rule a backend could satisfy by tearing everything down and still get
	// wrong: the frame is restarted, not abandoned, so this draw belongs to the
	// frame that is now running.
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

	harness.end_not_comparable();

	// The buffer is the size the resize asked for, and read_back_buffer agreed
	// with it - which is what makes the assertions below addressable at all.
	CHECK(harness.buffer_size().x == static_cast<float>(buffer_side / 2));
	CHECK(harness.buffer_size().y == static_cast<float>(buffer_side / 2));

	// WHAT WAS DRAWN BEFORE THE RESIZE IS GONE, AND WHAT WAS DRAWN AFTER IT IS
	// THERE. The first sprite covered 32x32 of a 64x64 buffer and the second
	// covers 8x8 of a 32x32 one, so a pixel at (20, 20) is inside the first and
	// outside the second: it is the one place the two disagree, and it says
	// which frame the buffer is holding.
	CHECK(harness.at(4, 4) == white_pixel);
	CHECK(harness.at(20, 20) == black_pixel);
}

TEST_CASE("CONTRACT: a resize before the first set_view_count restarts the frame")
{
	// THE SAME TERM AS THE CASE ABOVE, ARRIVING ONE CALL EARLIER, and it is a
	// separate case because a backend can pass that one and fail this one. The
	// interval renderer.h legislates starts at begin_frame; the part of it
	// before the first set_view_count is the part where NOTHING IS OPEN - no
	// view is recording, no list is holding a barrier - so a backend that
	// decides whether there is a frame to restart by asking what is open
	// answers "no frame" here and skips the restart. Both Direct3D backends
	// did, and it cost the new buffer its clear on both and its
	// PRESENT -> RENDER_TARGET barrier on D3D12.
	//
	// WHAT MAKES THE MISSING BARRIER FAIL RATHER THAN PASS IS THE DEBUG LAYER
	// AND NOT AN ASSERTION BELOW, which is worth saying because it is the
	// half of this case a reader cannot see: engine/render/d3d12/
	// device_resources.cpp asks the info queue to break on ERROR, and drawing
	// into a back buffer that is still in PRESENT is one. The assertions pin
	// what pixels can pin - the buffer is the new size, the draw that came
	// after the resize is in it, and the rest of it is the clear colour - and
	// a driver that happens to hand back zeroed memory would satisfy the last
	// of those either way. Both halves are the case; neither is the whole of
	// it.
	Harness harness;
	Renderer& renderer = harness.renderer();

	// begin_frame WITHOUT harness.begin(), which is the whole point of the
	// case: begin() calls set_view_count immediately after begin_frame, so
	// every other case in this file is past this interval before it can be
	// resized. It is not a contrived sequence - a shell that lays its views
	// out from the frame's own state does its arithmetic here.
	renderer.begin_frame();

	// The window first and then the renderer, in that order and for the reason
	// the case above gives: back_buffer_size is answered from the window on
	// the OpenGL backend and not on the other three, so a resize the window
	// never had would ask GL a different question from the rest.
	harness.resize_window(buffer_side / 2, buffer_side / 2);
	CHECK(renderer.window_size_changed(buffer_side / 2, buffer_side / 2));

	// And now the frame says what it has, which is the ordinary next call and
	// the one the resize arrived in front of.
	renderer.set_view_count(1);
	DrawList list = renderer.view(0);
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

	harness.end_not_comparable();

	CHECK(harness.buffer_size().x == static_cast<float>(buffer_side / 2));
	CHECK(harness.buffer_size().y == static_cast<float>(buffer_side / 2));

	// The 8x8 sprite is there, and the buffer around it is the clear colour
	// rather than whatever ResizeBuffers left in it.
	CHECK(harness.at(4, 4) == white_pixel);
	CHECK(harness.at(20, 20) == black_pixel);
}

TEST_CASE("CONTRACT: a frame may be read back and then presented")
{
	// THE INTERVAL renderer.h NAMES, WALKED TO ITS END. read_back_buffer says
	// it is called "BETWEEN submit() AND end_frame()", and this is the only
	// case that reaches the second half of that sentence: Harness::end
	// deliberately stops at the read, because presenting a flip-model swap
	// chain discards the buffer it just read. Without it the sequence the seam
	// explicitly permits - draw, submit, read, present - runs on no backend,
	// and a client that screenshots a frame and then shows it is doing exactly
	// that.
	//
	// IT WAS A LIVE DEFECT ON THE FIFTH BACKEND AND ONLY THE VALIDATION LAYERS
	// COULD SEE IT. A read-back there submits the frame's command buffer
	// mid-frame and waits, and end_frame then wants the same buffer again -
	// which is not a begin but a wait and a pool reset, because a command
	// buffer that has been to the queue is not in the state vkBeginCommandBuffer
	// takes (VUID-vkBeginCommandBuffer-commandBuffer-00050).
	// engine/render/vulkan/device_resources.cpp's commands() is where that is
	// answered. The other three had nothing to answer, which is the usual
	// shape of a term nobody had written down.
	//
	// WHAT IT ASSERTS IS THAT THE NEXT FRAME IS ORDINARY, because that is the
	// only observable half. What a present does to the buffer it presented is
	// a backend's business and differs - two of the four discard it - so the
	// statement worth pinning is that the frame after one is clean, correctly
	// cleared, and draws where it is told.
	Harness harness;

	DrawList first = harness.begin();
	first.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	CHECK(harness.at(4, 4) == white_pixel);
	CHECK(harness.at(20, 20) == black_pixel);

	// The half of the interval nothing else in this file walks.
	harness.renderer().end_frame();

	// And an ordinary frame after it, drawing somewhere the first one did not,
	// so that "the clear happened" and "the new sprite landed" are two
	// different pixels rather than one.
	DrawList second = harness.begin();
	second.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(16.0f, 16.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	CHECK(harness.at(20, 20) == white_pixel);
	CHECK(harness.at(4, 4) == black_pixel);
}

TEST_CASE("CONTRACT: a frame submitted and never presented contributes nothing to the next")
{
	// THIS NAMES A TERM RATHER THAN ADDING COVERAGE. Every case in this file
	// begins after a frame that was submitted and never presented, because
	// Harness::end stops at the read; what that relies on is the sentence at
	// renderer.h's begin_frame, and this is the case that says it.
	//
	// BUILT TO FAIL, which a full-frame colour over another full-frame colour
	// would not be: the second hides the first whatever leaked. Every frame
	// here draws the same TRANSLUCENT sprite in the same place, so a recording
	// that survived into the next frame - a submit flag left set, a deferred
	// list not drained, a command pool not reset - would composite a second
	// time and the bytes would move: half grey over itself is three-quarter
	// grey.
	//
	// AND IN THE ORDER A CAPTURE FROM INSIDE A RUNNING Application PRODUCES
	// (renderer.h, read_back_buffer): the out-of-band frame, read and never
	// presented; the Application's own frame, presented; then the next one.
	Harness harness;

	const RectangleF place(8.0f, 8.0f, 16.0f, 16.0f);
	const Colour half(0.5f, 0.5f, 0.5f, 0.5f);

	DrawList captured = harness.begin();
	captured.draw_sprite(harness.quad, Harness::white_texel(), place, half,
		0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	// Half of white over black: 127 or 128 by the device's rounding, and
	// nowhere near the 191 a doubled composite would make.
	const Pixel once = harness.at(16, 16);
	CHECK(once.r >= 126);
	CHECK(once.r <= 129);
	CHECK(once.g == once.r);
	CHECK(once.b == once.r);
	CHECK(harness.at(40, 40) == black_pixel);
	const std::vector<Pixel> expected = harness.snapshot();

	// The Application's frame, which a present ends.
	DrawList shown = harness.begin();
	shown.draw_sprite(harness.quad, Harness::white_texel(), place, half,
		0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();
	CHECK(harness.snapshot() == expected);
	harness.renderer().end_frame();

	// And the frame after it.
	DrawList next = harness.begin();
	next.draw_sprite(harness.quad, Harness::white_texel(), place, half,
		0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();
	CHECK(harness.snapshot() == expected);
}
