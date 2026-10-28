#pragma once

#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/texture_data.h"
#include "tests/render/test_window.h"

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

// Shared fixture for the pixel contract; state belongs to each Harness.
namespace render_tests::pixel
{
	inline constexpr int buffer_side = 64;

	struct Pixel
	{
		unsigned char r = 0;
		unsigned char g = 0;
		unsigned char b = 0;
		unsigned char a = 0;

		bool operator==(const Pixel& other) const;
	};

	// doctest prints these when an assertion fails. Without it a failure says
	// only that two objects differ, which is the least useful thing it could
	// say about a colour.
	std::ostream& operator<<(std::ostream& stream, const Pixel& pixel);

	inline constexpr Pixel black_pixel{ 0, 0, 0, 255 };
	inline constexpr Pixel red_pixel{ 255, 0, 0, 255 };
	inline constexpr Pixel green_pixel{ 0, 255, 0, 255 };
	inline constexpr Pixel blue_pixel{ 0, 0, 255, 255 };
	inline constexpr Pixel white_pixel{ 255, 255, 255, 255 };

	labrador::TextureData flat_texture(unsigned char red, unsigned char green,
		unsigned char blue);

	// A device, a table with one texture in it, and a frame you can read back.
	class Harness
	{
	public:
		Harness();

		Harness(const Harness&) = delete;
		Harness& operator=(const Harness&) = delete;

		// Returns view 0, because almost every case here wants one view and
		// wants it without saying so. The default and the signature are
		// null_tests.cpp's, deliberately: the two harnesses are the same shape
		// so that a multi-view case can be written once in each vocabulary -
		// one asserting what was recorded, the other what was rasterised.
		labrador::DrawList begin(int views = 1);

		labrador::DrawList view(int index);

		// For the cases that change the view count in the middle of a frame,
		// which is a state the seam names and nothing on a device has entered.
		labrador::Renderer& renderer();

		// For the cases that build their own texture rather than loading one:
		// the minified draw, the re-loaded name, and the two that finding 6 of
		// docs/review/backend-equivalence-2/ asked for. Both are
		// null_tests.cpp's accessors, spelt the same way.
		labrador::RenderResources& resources();

		// Deliberately no end_frame(). Presenting a FLIP_DISCARD swap chain
		// throws the back buffer's contents away, which is exactly what is
		// being read here. begin_frame clears and rebinds on the next pass, so
		// never presenting costs a test nothing.
		//
		// The read-then-present case below walks the far end of the interval
		// renderer.h gives read_back_buffer. The native sequences in
		// presentation_lifecycle_tests.cpp present without per-frame reads.
		void end();

		// Readback without a golden image. Three resize cases use this: one
		// has backend-dependent dimensions, and two deliberately leave the
		// reference set's 64x64 size. The analytic composition and viewport
		// regressions also use it, asserting every pixel against known colours.
		// Each call site states its reason; ordinary pixel cases use end().
		void end_not_comparable();

		// Changes the window's client area and TELLS THE RENDERER NOTHING,
		// which is not a contrived state: engine/app/win32/window.cpp discards
		// every WM_SIZE for the duration of a drag and renders a full frame
		// from WM_PAINT for every step of it, so a frame drawn into a window
		// whose size the renderer was never told is the normal case while a
		// user is dragging an edge.
		void resize_window(int width, int height);

		// What the last end() read back, and how big the renderer said it was.
		// Every case except the three that resize knows both are buffer_side;
		// at() goes through the reported width anyway so that the three which
		// do not can be written in the same vocabulary as the rest.
		labrador::Vector2F buffer_size() const;
		std::size_t byte_count() const;

		Pixel at(int x, int y) const;

		// The whole frame, for the two cases that compare one against another
		// rather than against a colour. Held by value so a second end() can
		// overwrite the buffer underneath.
		std::vector<Pixel> snapshot() const;

		// The smallest rectangle holding every pixel the clear did not leave
		// black, and the vocabulary every text case below is written in.
		//
		// Empty - width and height zero - when the frame is untouched, which is
		// itself an assertion two of the cases make.
		labrador::RectangleI ink_bounds() const;

		bool row_has_ink(int y) const;

		// What the layout half of the engine promises about the drawing half.
		// The text cases assert against this rather than against numbers read
		// out of the atlas, so each one says which term it is fixing instead of
		// what this particular font happens to measure.
		labrador::Vector2F measure(const std::wstring& text) const;

		// The whole 2x2 texture, and single texels out of it. Rectangles here
		// are (x, y, width, height), which is why a one-texel source is
		// (1, 1, 1, 1) and not (1, 1, 2, 2).
		static labrador::RectangleI whole();
		static labrador::RectangleI red_texel();
		static labrador::RectangleI white_texel();

		labrador::TextureHandle quad;
		labrador::FontHandle font;

	private:
		// The half of end() that both spellings of it share.
		void read_frame();

		// FIRST, SO IT IS THE LAST TO GO. The device was made on it, and a
		// context outliving the window or canvas it draws into is a state no
		// backend is written for.
		render_tests::TestWindow window_{ buffer_side, buffer_side };

		// THE TABLE BEFORE THE RENDERER, so it dies after one:
		// render_resources.h states it as a term of the seam. A sequence that
		// presents without a final read relies on the renderer's destructor
		// to finish using these textures before their table is released.
		labrador::RenderResources resources_;
		labrador::Renderer renderer_;
		std::vector<unsigned char> pixels_;
		labrador::Vector2F buffer_ = labrador::Vector2F(static_cast<float>(buffer_side),
			static_cast<float>(buffer_side));
	};
}
