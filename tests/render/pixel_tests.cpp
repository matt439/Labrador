#include "engine/render/renderer.h"
#include "engine/render/camera.h"
#include "engine/render/colour.h"
#include "engine/render/render_resources.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"

#include "tests/render/golden_image.h"

#include <doctest/doctest.h>

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <ostream>
#include <string>
#include <tuple>
#include <vector>

// The first test of anything this engine draws.
//
// WHY THIS CAN EXIST AT ALL. device_resources.cpp falls back to a WARP device
// in every build that is not NDEBUG, so a debug build makes a real Direct3D
// device with no GPU and no display attached - which means the whole drawing
// path is reachable from a test executable, and always was. What was missing
// was a way to see the result: Renderer::read_back_buffer, added with these
// tests, is what turns "the seam serves headless testing" from a claim into
// something a build can check.
//
// AND WHY IT DOES NOT MAKE THE NULL BACKEND UNNECESSARY. WARP is a D3D-only
// luxury: an in-box, fully conformant software rasteriser that no other API
// gets on Windows. A second backend either ships a software rasteriser of its
// own alongside it - Mesa's llvmpipe is not in the box - or these tests run
// against it only on a machine with a working driver. The null backend is
// still what makes the seam testable everywhere; this file is what tells it
// what the right answers are.
//
// WHAT THESE PIN. Every term below was decided by DirectXTK's defaults and
// written down nowhere, which means a second backend could differ on any of
// them and nothing would say so - the samples would simply look slightly
// wrong, and only to somebody who knew what to look for. They are executable
// now. A second backend is correct when this file passes against it, and each
// case names the term it fixes rather than describing what it does.
//
// WHAT THEY DO NOT PIN, said plainly rather than left to be discovered:
// rotation, non-uniform scale, the difference between point and linear
// filtering, flipped text, and the clamp that stops a glyph with a negative
// left bearing hanging off the start of its line - which no glyph in this
// font has, so no assertion here can reach it.
//
// AND EVERY FRAME IS ALSO CHECKED WHOLE. Each case's read-back is compared
// byte for byte against a PNG of it in tests/render/golden/, which is the one
// statement here that a difference BETWEEN two backends can fail - every
// assertion in this file holds one backend to a relationship, and two
// hand-copied implementations can get the same relationship wrong in the same
// direction without either noticing. tests/render/golden_image.h has the whole
// argument. Fifty-one frames; forty-eight of them are 64x64 on every backend
// and identical across the three that rasterise, and those forty-eight are the
// images in tests/render/golden/. The other three are not 64x64 - one because
// the seam makes its size backend-specific, two because they ask for a
// different size deliberately - and a golden set is one image per case at one
// size. Harness::end_not_comparable is where that is written down, and the
// count of images should be checkable against the count of frames from here:
// forty-eight and three.
//
// AND ONE MORE THAT IS NOT A GAP IN THE LIST BUT A PROPERTY OF THE METHOD.
// Because the text cases below are relationships (see two paragraphs down), a
// term added EQUALLY TO EVERY GLYPH cancels out of every one of them and this
// file cannot see it at all. The glyph's vertical bearing is exactly such a
// term: deleting it used to leave this file green on every backend that
// rasterises, the null backend's record test green, and the samples merely a
// few pixels wrong.
// It is pinned in tests/render/sprite_geometry_tests.cpp instead, as a
// difference between two glyphs whose bearings differ - which is the shape an
// absolute term has to be asked about, and which needs no device. Anything
// else uniform across a line belongs there for the same reason.
//
// TEXT IS PINNED BY THE SECOND HALF OF THIS FILE, and it is pinned as
// relationships rather than as colours. A sprite case above can assert that one
// pixel is exactly RED because the test texture has four flat texels in it. A
// glyph has neither property: its edges are anti-aliased, and the atlas a
// .spritefont carries is block-compressed, so the coverage value at any one
// pixel is a fact about the compressor. What a caller actually depends on is
// where the ink landed, how far the next character is from it, and that the
// box measure_text promised is the box that was filled - and every one of those
// is a statement about two rectangles, which survives a different rasteriser
// exactly as the sprite assertions do.
//
// THE TERMS BELOW WERE DECIDED BY DirectXTK'S SpriteFont AND WRITTEN DOWN
// NOWHERE, which is worse here than it is for sprites: the pen arithmetic is a
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

namespace
{
	using namespace labrador;
	using namespace mattmath;

	constexpr int BUFFER_SIZE = 64;

	// Registered once per process, on first use. Never shown: there is no
	// ShowWindow call anywhere in this file, so a test run puts nothing on
	// screen and steals no focus.
	HWND create_hidden_window()
	{
		static const wchar_t* CLASS_NAME = L"LabradorPixelTestWindow";

		static const ATOM atom = []() -> ATOM
			{
				WNDCLASSEXW window_class = {};
				window_class.cbSize = sizeof(WNDCLASSEXW);
				window_class.lpfnWndProc = DefWindowProcW;
				window_class.hInstance = GetModuleHandleW(nullptr);
				window_class.lpszClassName = CLASS_NAME;
				return RegisterClassExW(&window_class);
			}();
		REQUIRE(atom != 0);

		HWND window = CreateWindowExW(0, CLASS_NAME, L"pixel tests", WS_POPUP,
			0, 0, BUFFER_SIZE, BUFFER_SIZE, nullptr, nullptr,
			GetModuleHandleW(nullptr), nullptr);
		REQUIRE(window != nullptr);
		return window;
	}

	struct Pixel
	{
		unsigned char r = 0;
		unsigned char g = 0;
		unsigned char b = 0;
		unsigned char a = 0;

		bool operator==(const Pixel& other) const
		{
			return this->r == other.r && this->g == other.g &&
				this->b == other.b && this->a == other.a;
		}
	};

	// doctest prints these when an assertion fails. Without it a failure says
	// only that two objects differ, which is the least useful thing it could
	// say about a colour.
	std::ostream& operator<<(std::ostream& stream, const Pixel& pixel)
	{
		return stream << "rgba(" << static_cast<int>(pixel.r) << ", "
			<< static_cast<int>(pixel.g) << ", " << static_cast<int>(pixel.b)
			<< ", " << static_cast<int>(pixel.a) << ")";
	}

	constexpr Pixel BLACK{ 0, 0, 0, 255 };
	constexpr Pixel RED{ 255, 0, 0, 255 };
	constexpr Pixel GREEN{ 0, 255, 0, 255 };
	constexpr Pixel BLUE{ 0, 0, 255, 255 };
	constexpr Pixel WHITE{ 255, 255, 255, 255 };

	// Two by two, one colour, one level. For the case that cares which bytes a
	// name is holding rather than what an atlas looks like.
	TextureData flat_texture(unsigned char red, unsigned char green,
		unsigned char blue)
	{
		TextureData texture;
		texture.width = 2;
		texture.height = 2;
		texture.format = TextureFormat::r8g8b8a8_unorm;
		texture.levels.push_back(texture_level(texture.format, 2, 2, 0));
		texture.pixels.resize(texture.levels[0].size);

		for (size_t byte = 0; byte < texture.pixels.size(); byte += 4)
		{
			texture.pixels[byte] = red;
			texture.pixels[byte + 1] = green;
			texture.pixels[byte + 2] = blue;
			texture.pixels[byte + 3] = 255;
		}

		return texture;
	}

	// A texture that carries a mip chain, which no file in either client does.
	//
	// Level zero is sixteen pixels of red and level one is eight of blue, so
	// which level a minified draw sampled is the colour that comes back rather
	// than an inference from one. The seam's texture handover is a plain
	// aggregate (engine/render/texture_data.h), so this needs no file, no
	// tool and no new bytes in the repository - which is the only reason the
	// term below is testable at all, because there is nothing to load that
	// would exercise it.
	TextureData two_level_texture()
	{
		TextureData texture;
		texture.width = 16;
		texture.height = 16;
		texture.format = TextureFormat::r8g8b8a8_unorm;

		const TextureLevel coarse_offset =
			texture_level(texture.format, 16, 16, 0);
		texture.levels.push_back(coarse_offset);
		texture.levels.push_back(
			texture_level(texture.format, 8, 8, coarse_offset.size));

		const TextureLevel& fine = texture.levels[0];
		const TextureLevel& coarse = texture.levels[1];
		texture.pixels.resize(fine.size + coarse.size);

		for (size_t byte = fine.offset; byte < fine.offset + fine.size;
			byte += 4)
		{
			texture.pixels[byte] = 255;
			texture.pixels[byte + 3] = 255;
		}
		for (size_t byte = coarse.offset; byte < coarse.offset + coarse.size;
			byte += 4)
		{
			texture.pixels[byte + 2] = 255;
			texture.pixels[byte + 3] = 255;
		}

		return texture;
	}

	// A device, a table with one texture in it, and a frame you can read back.
	class Harness
	{
	public:
		Harness()
		{
			this->window_ = create_hidden_window();

			// FOUR VIEWS OF CAPACITY, WHICH IS THE SAME NUMBER null_tests.cpp
			// asks for, and it is not four because anything here draws four.
			// A capacity is a property of the layout - four for split-screen,
			// one for a sample - and the two files asking for the same one is
			// what lets a case be written in the same vocabulary in both. This
			// was 1 until the golden set made a multi-view frame checkable, and
			// while it was 1 every multi-view path in both backends - D3D11's
			// per-view deferred contexts, GL's per-view record-and-replay,
			// view order against call order - reached no rasteriser anywhere.
			this->renderer_.create_device(this->window_, BUFFER_SIZE,
				BUFFER_SIZE, 4);
			this->renderer_.set_resources(&this->resources_);

			// content/ is copied beside the executable by this folder's
			// CMakeLists, and ctest runs from the build directory.
			load_texture_asset(this->renderer_, this->resources_,
				"./content/", "quad");
			this->quad = this->resources_.resolve_texture("quad");

			// The font is a copy of the one both samples use rather than a
			// reference to it, for the same reason quad.dds is not a sample
			// texture: a sample that restyles itself must not be able to move a
			// pin in here. It is monospaced and bold, which is not decoration -
			// every character in it steps the pen by the same whole number of
			// pixels, so the advance cases below can be exact rather than
			// approximate.
			load_font_asset(this->renderer_, this->resources_, "./content/",
				"courier_new_bold_16");
			this->font = this->resources_.resolve_sprite_font(
				"courier_new_bold_16");
		}

		~Harness()
		{
			DestroyWindow(this->window_);
		}

		Harness(const Harness&) = delete;
		Harness& operator=(const Harness&) = delete;

		// Returns view 0, because almost every case here wants one view and
		// wants it without saying so. The default and the signature are
		// null_tests.cpp's, deliberately: the two harnesses are the same shape
		// so that a multi-view case can be written once in each vocabulary -
		// one asserting what was recorded, the other what was rasterised.
		DrawList begin(int views = 1)
		{
			this->renderer_.begin_frame();
			this->renderer_.set_view_count(views);
			return this->renderer_.view(0);
		}

		DrawList view(int index) { return this->renderer_.view(index); }

		// For the cases that change the view count in the middle of a frame,
		// which is a state the seam names and nothing on a device has entered.
		Renderer& renderer() { return this->renderer_; }

		// For the one case that builds its own texture rather than loading
		// one. Both are null_tests.cpp's accessors, spelt the same way.
		RenderResources& resources() { return this->resources_; }

		// Deliberately no end_frame(). Presenting a FLIP_DISCARD swap chain
		// throws the back buffer's contents away, which is exactly what is
		// being read here. begin_frame clears and rebinds on the next pass, so
		// never presenting costs a test nothing.
		void end()
		{
			this->read_frame();

			// And the frame itself is checked against the image of it that is
			// in the repository - the one statement in this file that holds one
			// backend to another rather than holding each separately to a
			// relationship they can both get wrong in the same direction
			// (tests/render/golden_image.h says why that gap exists at all).
			//
			// Here rather than in each case, so that the coverage grows with
			// the file instead of with a list somebody has to remember to
			// extend: a case added tomorrow is golden tomorrow.
			golden::check_frame(static_cast<int>(this->buffer_.x),
				static_cast<int>(this->buffer_.y), this->pixels_);
		}

		// end(), for the frames in this file that no golden image can hold -
		// and it is the seam that says so, not a tolerance this harness needed.
		//
		// THERE ARE THREE OF THEM AND THEY ARE EXEMPT FOR THE SAME REASON:
		// none is 64x64. One is the drag-resize below, whose buffer is a
		// different size on different backends by contract; the other two are
		// the mid-frame resize cases at the end of this file, which ask for a
		// 32x32 buffer deliberately and get it on every backend. A golden set
		// is one image per case at one size, so a case that changes the size
		// mid-frame has no image to be - and all three assert on pixels they
		// address themselves, which is what a golden image would have added
		// nothing to.
		//
		// THE FIRST IS EXEMPT FOR A STRONGER REASON THAN THE OTHER TWO, and
		// the difference is worth keeping. renderer.h, back_buffer_size: "a
		// swap chain does not follow its window, so both Direct3D backends
		// answer the size they were told and let Present stretch; a WGL
		// context's default framebuffer is the window's client area, so the GL
		// backend answers the window." A frame read back while the two
		// disagree is therefore 64x64 on some backends and 64x48 on others BY
		// CONTRACT, and one file cannot be both - so no image could hold it
		// however the set were organised. The other two could be held to an
		// image and are not; that is a choice about how much of the newest
		// path belongs in a set whose every other member is one size, not a
		// fact about the seam. Every frame here that is not one of these three
		// is 64x64 on every backend and is compared byte for byte.
		//
		// Named rather than filtered by slug inside golden_image.cpp, so that
		// the reason sits with the case that needs it and renaming the case
		// cannot quietly move the exemption somewhere else.
		void end_not_comparable()
		{
			this->read_frame();
		}

		// Changes the window's client area and TELLS THE RENDERER NOTHING,
		// which is not a contrived state: engine/app/window.cpp discards every
		// WM_SIZE for the duration of a drag and renders a full frame from
		// WM_PAINT for every step of it, so a frame drawn into a window whose
		// size the renderer was never told is the normal case while a user is
		// dragging an edge.
		void resize_window(int width, int height)
		{
			REQUIRE(SetWindowPos(this->window_, nullptr, 0, 0, width, height,
				SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != 0);
		}

		// What the last end() read back, and how big the renderer said it was.
		// Every case but the three that resize knows both are BUFFER_SIZE;
		// at() goes through the reported width anyway so that the three which
		// do not can be written in the same vocabulary as the rest.
		Vector2F buffer_size() const { return this->buffer_; }
		size_t byte_count() const { return this->pixels_.size(); }

		Pixel at(int x, int y) const
		{
			const size_t stride = static_cast<size_t>(this->buffer_.x) * 4;
			const size_t offset =
				static_cast<size_t>(y) * stride + static_cast<size_t>(x) * 4;
			// Guarded rather than asserted unconditionally: ink_bounds walks
			// every pixel of every text case, and an assertion per read would
			// bury the file's real assertion count under four thousand.
			if (offset + 3 >= this->pixels_.size())
			{
				REQUIRE_MESSAGE(false, "pixel (", x, ", ", y,
					") is outside the back buffer");
				return {};
			}
			return { this->pixels_[offset], this->pixels_[offset + 1],
				this->pixels_[offset + 2], this->pixels_[offset + 3] };
		}

		// The whole frame, for the two cases that compare one against another
		// rather than against a colour. Held by value so a second end() can
		// overwrite the buffer underneath.
		std::vector<Pixel> snapshot() const
		{
			std::vector<Pixel> frame;
			frame.reserve(BUFFER_SIZE * BUFFER_SIZE);
			for (int y = 0; y < BUFFER_SIZE; y++)
			{
				for (int x = 0; x < BUFFER_SIZE; x++)
				{
					frame.push_back(this->at(x, y));
				}
			}
			return frame;
		}

		// The smallest rectangle holding every pixel the clear did not leave
		// black, and the vocabulary every text case below is written in.
		//
		// Empty - width and height zero - when the frame is untouched, which is
		// itself an assertion two of the cases make.
		RectangleI ink_bounds() const
		{
			int left = BUFFER_SIZE;
			int top = BUFFER_SIZE;
			int right = -1;
			int bottom = -1;

			for (int y = 0; y < BUFFER_SIZE; y++)
			{
				for (int x = 0; x < BUFFER_SIZE; x++)
				{
					if (this->at(x, y) == BLACK)
					{
						continue;
					}
					// Spelt out rather than through std::min and std::max,
					// which are macros here: this file includes <Windows.h>
					// for the window it never shows.
					if (x < left) { left = x; }
					if (y < top) { top = y; }
					if (x > right) { right = x; }
					if (y > bottom) { bottom = y; }
				}
			}

			if (right < 0)
			{
				return RectangleI::ZERO;
			}
			// Right and bottom are exclusive here, as they are everywhere else
			// in this file - RectangleI is (x, y, width, height).
			return RectangleI(left, top, right - left + 1, bottom - top + 1);
		}

		bool row_has_ink(int y) const
		{
			for (int x = 0; x < BUFFER_SIZE; x++)
			{
				if (!(this->at(x, y) == BLACK))
				{
					return true;
				}
			}
			return false;
		}

		// What the layout half of the engine promises about the drawing half.
		// The text cases assert against this rather than against numbers read
		// out of the atlas, so each one says which term it is fixing instead of
		// what this particular font happens to measure.
		Vector2F measure(const std::wstring& text) const
		{
			return this->resources_.measure_text(this->font, text);
		}

		// The whole 2x2 texture, and single texels out of it. Rectangles here
		// are (x, y, width, height), which is why a one-texel source is
		// (1, 1, 1, 1) and not (1, 1, 2, 2).
		static RectangleI whole() { return RectangleI(0, 0, 2, 2); }
		static RectangleI red_texel() { return RectangleI(0, 0, 1, 1); }
		static RectangleI white_texel() { return RectangleI(1, 1, 1, 1); }

		TextureHandle quad;
		FontHandle font;

	private:
		// The half of end() that both spellings of it share.
		void read_frame()
		{
			this->renderer_.submit();
			this->renderer_.read_back_buffer(this->pixels_);
			this->buffer_ = this->renderer_.back_buffer_size();
		}

		HWND window_ = nullptr;

		// THE TABLE BEFORE THE RENDERER, so it dies after one - render_
		// resources.h states it as a term of the seam. This harness was saved
		// from needing it only by accident: every end() routes through
		// read_back_buffer, which drains the GPU, so no frame here has ever
		// been in flight at the point the members go. The first case that
		// submits a frame it does not read back would have found it.
		RenderResources resources_;
		Renderer renderer_;
		std::vector<unsigned char> pixels_;
		Vector2F buffer_ = Vector2F(static_cast<float>(BUFFER_SIZE),
			static_cast<float>(BUFFER_SIZE));
	};
}

TEST_CASE("a frame nobody drew into is cleared to opaque black")
{
	Harness harness;

	std::ignore = harness.begin();
	harness.end();

	CHECK(harness.at(0, 0) == BLACK);
	CHECK(harness.at(BUFFER_SIZE / 2, BUFFER_SIZE / 2) == BLACK);
	CHECK(harness.at(BUFFER_SIZE - 1, BUFFER_SIZE - 1) == BLACK);
}

TEST_CASE("CONTRACT: the destination is in pixels, y runs down, origin top left")
{
	Harness harness;

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 4.0f, 4.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);
	harness.end();

	// A 4x4 sprite at the origin covers the top-left corner and nothing else.
	CHECK(harness.at(0, 0) == WHITE);
	CHECK(harness.at(3, 3) == WHITE);
	CHECK(harness.at(4, 4) == BLACK);

	// If y ran up, this is where it would be instead.
	CHECK(harness.at(2, BUFFER_SIZE - 2) == BLACK);
}

TEST_CASE("CONTRACT: texel (0,0) draws at the top left of the destination")
{
	Harness harness;

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);
	harness.end();

	// The single assertion a y-inverted texture coordinate fails, and the
	// reason the test texture has four different corners.
	CHECK(harness.at(2, 2) == RED);
	CHECK(harness.at(6, 2) == GREEN);
	CHECK(harness.at(2, 6) == BLUE);
	CHECK(harness.at(6, 6) == WHITE);
}

TEST_CASE("CONTRACT: flip mirrors the texture, not the destination rectangle")
{
	Harness harness;

	SUBCASE("horizontal swaps left and right, leaving top and bottom")
	{
		DrawList list = harness.begin();
		list.draw_sprite(harness.quad, Harness::whole(),
			RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
			Vector2F::ZERO, SpriteFlip::horizontal, 0.0f);
		harness.end();

		CHECK(harness.at(2, 2) == GREEN);
		CHECK(harness.at(6, 2) == RED);
		CHECK(harness.at(2, 6) == WHITE);
		CHECK(harness.at(6, 6) == BLUE);
	}

	SUBCASE("vertical swaps top and bottom, leaving left and right")
	{
		DrawList list = harness.begin();
		list.draw_sprite(harness.quad, Harness::whole(),
			RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
			Vector2F::ZERO, SpriteFlip::vertical, 0.0f);
		harness.end();

		CHECK(harness.at(2, 2) == BLUE);
		CHECK(harness.at(6, 2) == WHITE);
		CHECK(harness.at(2, 6) == RED);
		CHECK(harness.at(6, 6) == GREEN);
	}

	SUBCASE("both is the same as one after the other")
	{
		DrawList list = harness.begin();
		list.draw_sprite(harness.quad, Harness::whole(),
			RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
			Vector2F::ZERO, SpriteFlip::both, 0.0f);
		harness.end();

		CHECK(harness.at(2, 2) == WHITE);
		CHECK(harness.at(6, 2) == BLUE);
		CHECK(harness.at(2, 6) == GREEN);
		CHECK(harness.at(6, 6) == RED);
	}
}

TEST_CASE("CONTRACT: a positive rotation turns the sprite clockwise on screen")
{
	Harness harness;

	// Half of pi. Not exactly, because nothing is: std::cos of it is about
	// 6e-17 rather than 0, so the far corners land a hair off the whole pixels
	// named below and the fill rule decides which side of each they fall. That
	// is the point of the case as much as the direction is - see below.
	const float quarter_turn = 1.5707963f;

	// A bar four pixels tall and sixteen wide, pivoting on its own top-left
	// corner, because a square rotated about its centre looks the same both
	// ways round and every other case in this file passes 0.0f here.
	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::red_texel(),
		RectangleF(32.0f, 32.0f, 16.0f, 4.0f), Colour::white, quarter_turn,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);
	harness.end();

	// x' = x cos - y sin, y' = x sin + y cos, with y running DOWN the screen -
	// so +x turns towards +y, which is right turning to down. The bar was
	// pointing right and now points down, four pixels wide, hanging to the
	// left of the pivot because its own height went that way.
	CHECK(harness.at(30, 40) == RED);
	CHECK(harness.at(30, 46) == RED);

	// Where it was before the turn, and where an anticlockwise convention
	// would have put it. Either being red is a backend that turned the other
	// way, which no assertion in this file could previously have noticed.
	CHECK(harness.at(40, 34) == BLACK);
	CHECK(harness.at(30, 24) == BLACK);

	// AND THIS IS THE ONLY FRAME IN THE FILE WITH EDGES THAT ARE NOT AXIS
	// ALIGNED, which is worth more than the four assertions above.
	// build_sprite_quad truncates every edge to a whole pixel, so an
	// unrotated sprite never puts one across a pixel centre and the fill rule
	// is never consulted; a turned one puts all four there at once. D3D11
	// mandates the top-left rule exactly and GL 3.3 asks only that a shared
	// edge be neither dropped nor drawn twice, leaving the tie-break to the
	// implementation - so this is a term the two specifications do NOT agree
	// on, and the golden image is what says whether the two devices do.
	CHECK(harness.ink_bounds() == RectangleI(28, 32, 4, 16));
}

TEST_CASE("CONTRACT: the source rectangle selects, and is in texels")
{
	Harness harness;

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::red_texel(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);
	harness.end();

	// One texel stretched over the whole destination, so every sample is it.
	CHECK(harness.at(1, 1) == RED);
	CHECK(harness.at(6, 6) == RED);
}

TEST_CASE("CONTRACT: a minified draw samples level zero, not the chain")
{
	Harness harness;

	// Sixteen source pixels into four, so the level a mip-sampling backend
	// would pick is level one and not a near miss: log2(16/4) is 2, and the
	// chain stops at 1, so a sampler walking it lands on blue with margin.
	add_texture_asset(harness.renderer(), harness.resources(), "two_level",
		two_level_texture());
	const TextureHandle chained =
		harness.resources().resolve_texture("two_level");

	// BOTH FILTERS, because the term belongs to both samplers and because
	// TextureFilter::linear reached no device anywhere in this repository
	// until this case. Point and linear differ in which texels of a level
	// they combine; neither chooses a level.
	SUBCASE("under the point filter")
	{
		DrawList list = harness.begin();
		list.set_filter(TextureFilter::point);
		list.draw_sprite(chained, RectangleI(0, 0, 16, 16),
			RectangleF(8.0f, 8.0f, 4.0f, 4.0f), Colour::white, 0.0f,
			Vector2F::ZERO, SpriteFlip::none, 0.0f);
		harness.end();

		CHECK(harness.at(9, 9) == RED);
		CHECK(harness.at(10, 10) == RED);
	}

	SUBCASE("and under the linear filter")
	{
		DrawList list = harness.begin();
		list.set_filter(TextureFilter::linear);
		list.draw_sprite(chained, RectangleI(0, 0, 16, 16),
			RectangleF(8.0f, 8.0f, 4.0f, 4.0f), Colour::white, 0.0f,
			Vector2F::ZERO, SpriteFlip::none, 0.0f);
		harness.end();

		// Level zero is one flat colour, so linear filtering has nothing to
		// blend towards and the answer is exact rather than approximately red.
		// That is the whole reason the two levels are flat.
		CHECK(harness.at(9, 9) == RED);
		CHECK(harness.at(10, 10) == RED);
	}
}

TEST_CASE("CONTRACT: the tint multiplies the texel")
{
	Harness harness;

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour(1.0f, 0.0f, 0.0f, 1.0f),
		0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);
	harness.end();

	// White texel, red tint. A tint that replaced rather than multiplied would
	// give the same answer here, which is why the second draw below puts a
	// green tint on a red texel.
	CHECK(harness.at(4, 4) == RED);

	DrawList second = harness.begin();
	second.draw_sprite(harness.quad, Harness::red_texel(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour(0.0f, 1.0f, 0.0f, 1.0f),
		0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);
	harness.end();

	// Red texel times green tint is black. Replacement would give green.
	CHECK(harness.at(4, 4) == BLACK);
}

TEST_CASE("CONTRACT: the blend equation is premultiplied alpha")
{
	Harness harness;

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour(1.0f, 1.0f, 1.0f, 0.5f),
		0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);
	harness.end();

	// THE TERM MOST LIKELY TO BE GOT WRONG, and the one a screenshot will not
	// settle because both answers look plausible.
	//
	// The source factor is ONE, not SRC_ALPHA. A white texel under a half-alpha
	// tint is (1,1,1,0.5) going in, so the result over black is 1*(1,1,1) +
	// (1-0.5)*(0,0,0) = white. Straight alpha would give mid grey. The engine
	// therefore expects textures whose colour is already multiplied by their
	// alpha, and a backend that picks SRC_ALPHA here is wrong even though every
	// opaque sprite in both samples would still look exactly right.
	CHECK(harness.at(4, 4) == WHITE);
}

TEST_CASE("CONTRACT: a fractional destination truncates, it does not round")
{
	Harness harness;

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(10.9f, 0.0f, 8.0f, 4.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);
	harness.end();

	// Left edge 10.9 becomes 10. Rounding would put it at 11 and leave this
	// pixel black.
	CHECK(harness.at(9, 2) == BLACK);
	CHECK(harness.at(10, 2) == WHITE);

	// Right edge 18.9 becomes 18, and is exclusive.
	CHECK(harness.at(17, 2) == WHITE);
	CHECK(harness.at(18, 2) == BLACK);
}

TEST_CASE("CONTRACT: origin is in unscaled source texels")
{
	Harness harness;

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(8.0f, 8.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F(2.0f, 2.0f), SpriteFlip::none, 0.0f);
	harness.end();

	// An 8x8 destination over a 2x2 source is a scale of four, so an origin of
	// two source texels shifts the sprite eight destination pixels up and left
	// - putting its top-left corner exactly at (0,0).
	//
	// If origin were measured in destination pixels the shift would be two, the
	// sprite would sit at (6,6), and (1,1) would be black.
	CHECK(harness.at(1, 1) == RED);
	CHECK(harness.at(6, 1) == GREEN);
	CHECK(harness.at(1, 6) == BLUE);
	CHECK(harness.at(8, 8) == BLACK);
}

TEST_CASE("CONTRACT: layer_depth does not order draws, call order does")
{
	Harness harness;

	DrawList list = harness.begin();

	// Red first at the front, green second at the back. Depth sorting would
	// leave red on top; the batch does not sort, so the later call wins.
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour(1.0f, 0.0f, 0.0f, 1.0f),
		0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour(0.0f, 1.0f, 0.0f, 1.0f),
		0.0f, Vector2F::ZERO, SpriteFlip::none, 1.0f);

	harness.end();

	// A backend that honoured layer_depth would answer RED here, and that is a
	// difference in behaviour rather than in rounding - so if depth is ever
	// made to mean something, this is the test to change deliberately rather
	// than the one that starts failing.
	CHECK(harness.at(4, 4) == GREEN);
}

TEST_CASE("CONTRACT: the camera maps the destination, and changes mid list")
{
	Harness harness;

	DrawList list = harness.begin();

	// view = (world - translation) * scale, so this puts world (4,4) at the
	// top-left of the view.
	list.set_camera(Camera(4.0f, 4.0f, 1.0f));
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(4.0f, 4.0f, 4.0f, 4.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);

	// Back to screen space mid-list, which is what a HUD over a world does and
	// what set_camera being per range rather than per view exists for.
	list.set_camera(Camera::DEFAULT_CAMERA);
	list.draw_sprite(harness.quad, Harness::red_texel(),
		RectangleF(32.0f, 32.0f, 4.0f, 4.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);

	harness.end();

	// The camera moved the world sprite to the origin.
	CHECK(harness.at(1, 1) == WHITE);
	CHECK(harness.at(5, 5) == BLACK);

	// The screen-space sprite ignored it.
	CHECK(harness.at(33, 33) == RED);
}

// --- text ------------------------------------------------------------------

TEST_CASE("CONTRACT: the position is the top left of the text, and y runs down")
{
	Harness harness;

	// The string is two characters so that the box has a width the advance
	// contributes to, and both are capitals so nothing descends below the
	// baseline - a descender is the next case's business.
	const Vector2F size = harness.measure(L"AB");

	SUBCASE("every pixel of it is inside the box measure_text promised")
	{
		DrawList list = harness.begin();
		list.draw_text(harness.font, L"AB", Vector2F(12.0f, 8.0f),
			Colour::white, 1.0f, 0.0f, Vector2F::ZERO, 0.0f);
		harness.end();

		const RectangleI ink = harness.ink_bounds();
		CHECK(ink.width > 0);

		// THIS IS THE TERM EVERY LAYOUT IN BOTH CLIENTS RESTS ON. A widget that
		// centres a label, a menu that sizes a row to its longest entry and the
		// drop shadow that offsets a copy of a string all ask measure_text for
		// a size and then trust draw_text to stay inside it. Nothing checked
		// that they agree, and they are computed by two different functions -
		// so a backend could measure with the line spacing and draw with the
		// glyph height and every label would be right until one of them
		// clipped.
		const RectangleI promised(12, 8,
			static_cast<int>(std::ceil(size.x)),
			static_cast<int>(std::ceil(size.y)));
		CHECK(promised.contains(ink));

		// And it is not merely inside it: the left edge of the first glyph is
		// the left edge of the box plus that glyph's own bearing, which is
		// small. A backend that centred the text in the box, or measured from
		// the baseline, would still pass the containment above.
		CHECK(ink.left() < 12 + 6);
		CHECK(ink.top() < 8 + 6);
	}

	SUBCASE("moving the position down moves the text down")
	{
		DrawList high = harness.begin();
		high.draw_text(harness.font, L"AB", Vector2F(12.0f, 8.0f),
			Colour::white, 1.0f, 0.0f, Vector2F::ZERO, 0.0f);
		harness.end();
		const RectangleI first = harness.ink_bounds();

		DrawList low = harness.begin();
		low.draw_text(harness.font, L"AB", Vector2F(12.0f, 28.0f),
			Colour::white, 1.0f, 0.0f, Vector2F::ZERO, 0.0f);
		harness.end();
		const RectangleI second = harness.ink_bounds();

		// Twenty pixels down, not twenty up. If y ran the other way for text -
		// which is the axis a second backend is most likely to get wrong,
		// because its own clip space runs up - this is negative.
		CHECK(second.top() - first.top() == 20);
		CHECK(second.left() == first.left());
	}
}

TEST_CASE("CONTRACT: the pen advances by what measure_text says it does")
{
	Harness harness;

	const Vector2F position(6.0f, 20.0f);

	DrawList single = harness.begin();
	single.draw_text(harness.font, L"A", position, Colour::white, 1.0f, 0.0f,
		Vector2F::ZERO, 0.0f);
	harness.end();
	const RectangleI one = harness.ink_bounds();

	DrawList doubled = harness.begin();
	doubled.draw_text(harness.font, L"AA", position, Colour::white, 1.0f, 0.0f,
		Vector2F::ZERO, 0.0f);
	harness.end();
	const RectangleI two = harness.ink_bounds();

	REQUIRE(one.width > 0);

	// A SECOND CHARACTER ADDS AN ADVANCE AND NOTHING ELSE. The second A sits on
	// the same rows as the first, so the box grows to the right and in no other
	// direction.
	CHECK(two.left() == one.left());
	CHECK(two.top() == one.top());
	CHECK(two.bottom() == one.bottom());

	// AND THE ADVANCE IS THE ONE MEASURE REPORTS, which is the only thing
	// making measure_text usable for anything but a single glyph. Exact rather
	// than within a pixel because this font is monospaced and its step is a
	// whole number; a proportional font would need a tolerance here and the
	// term would be no different.
	const int advance = static_cast<int>(
		harness.measure(L"AA").x - harness.measure(L"A").x);
	CHECK(advance > 0);
	CHECK(two.right() - one.right() == advance);
}

TEST_CASE("CONTRACT: a space advances the pen and draws nothing")
{
	Harness harness;

	const Vector2F position(4.0f, 20.0f);

	SUBCASE("a string of spaces marks the frame nowhere")
	{
		DrawList list = harness.begin();
		list.draw_text(harness.font, L"   ", position, Colour::white, 1.0f,
			0.0f, Vector2F::ZERO, 0.0f);
		harness.end();

		// THE ATLAS HAS A GLYPH FOR U+0020 AND IT IS NOT EMPTY. MakeSpriteFont
		// writes a one-texel subrect for it, and that texel is not guaranteed
		// to be transparent - so a backend that drew every glyph the pen walked
		// over would put a row of dots on the screen wherever a caller wrote a
		// space. Whitespace is skipped at draw time and only at draw time.
		CHECK(harness.ink_bounds() == RectangleI::ZERO);
	}

	SUBCASE("but the character after it has moved")
	{
		DrawList near_list = harness.begin();
		near_list.draw_text(harness.font, L"AA", position, Colour::white, 1.0f,
			0.0f, Vector2F::ZERO, 0.0f);
		harness.end();
		const RectangleI adjacent = harness.ink_bounds();

		DrawList far_list = harness.begin();
		far_list.draw_text(harness.font, L"A A", position, Colour::white, 1.0f,
			0.0f, Vector2F::ZERO, 0.0f);
		harness.end();
		const RectangleI spaced = harness.ink_bounds();

		// One more cell wide, and it starts in the same place.
		const int advance = static_cast<int>(
			harness.measure(L"AA").x - harness.measure(L"A").x);
		CHECK(spaced.left() == adjacent.left());
		CHECK(spaced.right() - adjacent.right() == advance);
	}
}

TEST_CASE("CONTRACT: a newline returns to the left margin and drops a line")
{
	Harness harness;

	const Vector2F position(4.0f, 4.0f);

	DrawList single = harness.begin();
	single.draw_text(harness.font, L"A", position, Colour::white, 1.0f, 0.0f,
		Vector2F::ZERO, 0.0f);
	harness.end();
	const RectangleI one_line = harness.ink_bounds();

	DrawList wrapped = harness.begin();
	wrapped.draw_text(harness.font, L"A\nA", position, Colour::white, 1.0f,
		0.0f, Vector2F::ZERO, 0.0f);
	harness.end();
	const RectangleI two_lines = harness.ink_bounds();

	// Everything below compares two boxes, and two empty boxes agree about
	// everything. Say that both were drawn before believing they match.
	REQUIRE(one_line.width > 0);
	REQUIRE(two_lines.height > one_line.height);

	// Back to the margin, not to wherever the pen had reached.
	CHECK(two_lines.left() == one_line.left());
	CHECK(two_lines.right() == one_line.right());
	CHECK(two_lines.top() == one_line.top());

	// THE LINE SPACING IS THE FONT'S, NOT THE GLYPH'S. Courier at this size
	// leaves eleven blank rows between two capitals, so a backend that stepped
	// by the glyph height would set the two lines touching and nothing would
	// throw - it would just look wrong in a way that reads as a font choice.
	// measure_text is asked what the step is, one pixel of tolerance because
	// the spacing is fractional and the second line therefore starts between
	// two rows.
	const float spacing = harness.measure(L"A\nA").y - harness.measure(L"A").y;
	CHECK(spacing > 0.0f);
	CHECK(std::abs(static_cast<float>(two_lines.bottom() - one_line.bottom())
		- spacing) <= 1.0f);

	// And there is a gap, so the two lines are two lines.
	CHECK_FALSE(harness.row_has_ink(one_line.bottom() + 1));
}

TEST_CASE("CONTRACT: the tint multiplies the glyph, as it does a sprite")
{
	Harness harness;

	const Vector2F position(8.0f, 20.0f);

	DrawList plain = harness.begin();
	plain.draw_text(harness.font, L"Ag", position, Colour::white, 1.0f, 0.0f,
		Vector2F::ZERO, 0.0f);
	harness.end();
	const std::vector<Pixel> white_text = harness.snapshot();

	DrawList tinted = harness.begin();
	tinted.draw_text(harness.font, L"Ag", position,
		Colour(1.0f, 0.0f, 0.0f, 1.0f), 1.0f, 0.0f, Vector2F::ZERO, 0.0f);
	harness.end();
	const std::vector<Pixel> red_text = harness.snapshot();

	// EVERY PIXEL AT ONCE, AND AGGREGATED INTO THREE ASSERTIONS. A glyph's
	// coverage differs pixel by pixel and this file has no business knowing
	// what any one of those values is; what it can say is that multiplying by
	// pure red leaves the red channel exactly alone and empties the other two.
	// A tint that replaced rather than multiplied would flood the whole glyph
	// box with solid red and fail the first of these.
	int ink_pixels = 0;
	bool red_preserved = true;
	bool other_channels_cleared = true;

	for (size_t i = 0; i < white_text.size(); i++)
	{
		if (white_text[i] == BLACK)
		{
			// Nothing was drawn here, so nothing may appear here either.
			other_channels_cleared = other_channels_cleared &&
				red_text[i] == BLACK;
			continue;
		}
		ink_pixels++;
		red_preserved = red_preserved && red_text[i].r == white_text[i].r;
		other_channels_cleared = other_channels_cleared &&
			red_text[i].g == 0 && red_text[i].b == 0;
	}

	CHECK(ink_pixels > 0);
	CHECK(red_preserved);
	CHECK(other_channels_cleared);
}

TEST_CASE("CONTRACT: text scales about the origin, in unscaled text pixels")
{
	Harness harness;

	const Vector2F position(24.0f, 20.0f);

	DrawList plain_list = harness.begin();
	plain_list.draw_text(harness.font, L"A", position, Colour::white, 1.0f,
		0.0f, Vector2F::ZERO, 0.0f);
	harness.end();
	const RectangleI plain = harness.ink_bounds();

	DrawList doubled_list = harness.begin();
	doubled_list.draw_text(harness.font, L"A", position, Colour::white, 2.0f,
		0.0f, Vector2F::ZERO, 0.0f);
	harness.end();
	const RectangleI doubled = harness.ink_bounds();

	REQUIRE(plain.width > 0);

	// Twice the size, and pinned at the position rather than about its own
	// centre. Exact: point sampling at a whole-number scale gives every texel
	// exactly two pixels, so the anti-aliased edge doubles along with the rest.
	CHECK(doubled.width == plain.width * 2);
	CHECK(doubled.height == plain.height * 2);
	CHECK(doubled.left() - static_cast<int>(position.x) ==
		(plain.left() - static_cast<int>(position.x)) * 2);
	CHECK(doubled.top() - static_cast<int>(position.y) ==
		(plain.top() - static_cast<int>(position.y)) * 2);

	DrawList shifted = harness.begin();
	shifted.draw_text(harness.font, L"A", position, Colour::white, 2.0f, 0.0f,
		Vector2F(8.0f, 4.0f), 0.0f);
	harness.end();
	const RectangleI moved = harness.ink_bounds();

	// THE SAME TERM THE SPRITE CASE FIXES, and it is worth fixing twice because
	// the two travel through different arithmetic: a sprite's origin is in
	// source texels and is scaled by the ratio of destination to source, while
	// text has no destination rectangle at all and its origin is in the
	// unscaled pen space measure_text reports in. An origin of eight text
	// pixels at a scale of two moves the text sixteen screen pixels, not eight.
	CHECK(moved.left() == doubled.left() - 16);
	CHECK(moved.top() == doubled.top() - 8);
	CHECK(moved.width == doubled.width);
}

TEST_CASE("CONTRACT: the camera maps text position and text scale together")
{
	Harness harness;

	DrawList screen = harness.begin();
	screen.draw_text(harness.font, L"A", Vector2F(6.0f, 10.0f), Colour::white,
		2.0f, 0.0f, Vector2F::ZERO, 0.0f);
	harness.end();
	const RectangleI direct = harness.ink_bounds();

	DrawList world = harness.begin();

	// view = (world - translation) * scale, so world (19, 13) under this camera
	// is screen (6, 10) and a text scale of one is a screen scale of two.
	world.set_camera(Camera(16.0f, 8.0f, 2.0f));
	world.draw_text(harness.font, L"A", Vector2F(19.0f, 13.0f), Colour::white,
		1.0f, 0.0f, Vector2F::ZERO, 0.0f);
	harness.end();
	const RectangleI through_camera = harness.ink_bounds();

	// As in the newline case: two empty boxes are equal, so check there is
	// something to compare before comparing it.
	REQUIRE(direct.width > 0);
	REQUIRE(through_camera.width > 0);

	// BOTH HALVES, WHICH IS WHY THE CAMERA HAS A ZOOM AT ALL. A backend that
	// applied the camera to the position and forgot the scale would put the
	// text in exactly the right place at exactly the wrong size, which looks
	// like a font problem and is not one.
	CHECK(through_camera.left() == direct.left());
	CHECK(through_camera.top() == direct.top());
	CHECK(through_camera.width == direct.width);
	CHECK(through_camera.height == direct.height);
}

TEST_CASE("CONTRACT: a character the font lacks draws the stand-in glyph")
{
	Harness harness;

	const Vector2F position(20.0f, 20.0f);

	DrawList question = harness.begin();
	question.draw_text(harness.font, L"?", position, Colour::white, 1.0f, 0.0f,
		Vector2F::ZERO, 0.0f);
	harness.end();
	const std::vector<Pixel> expected = harness.snapshot();

	DrawList curly = harness.begin();
	// U+2019, the character a word processor inserts for an apostrophe. It is
	// outside the 32..126 region MakeSpriteFont writes when nobody chooses one,
	// which is every .spritefont in this tree. Spelt as an escape so that this
	// file stays ASCII and no encoding decision can change what is tested.
	curly.draw_text(harness.font, L"\u2019", position, Colour::white, 1.0f,
		0.0f, Vector2F::ZERO, 0.0f);
	harness.end();
	const std::vector<Pixel> drawn = harness.snapshot();

	// NOT AN EXCEPTION AND NOT A HOLE. Before the loader installed a default
	// character this threw on the frame that first drew the string, several
	// screens from wherever the text was read. The seam's own promise, that
	// text about to be drawn shows mojibake rather than vanishing, is this
	// assertion: the frame is pixel for pixel the frame a question mark draws.
	CHECK(harness.ink_bounds().width > 0);
	CHECK((drawn == expected));
}

// --- the viewport -----------------------------------------------------------
//
// NOTHING HERE CALLED set_viewport UNTIL NOW, which is why the two device
// backends were able to disagree about a pane for as long as they did. The
// harness above builds one integral 64x64 view, both samples take their
// resolution from a Vector2I, and the seam's only production caller of
// set_viewport is engine/scene/scene.cpp, which no test reaches. So the pane
// arithmetic was exercised by neither the pixel contract nor the recording.
//
// THE RULE ITSELF IS PINNED HEADLESSLY, in tests/render/viewport_tests.cpp,
// which runs in every configuration including the null one. What that file
// cannot see is whether a backend USES it - that is one line in each of two
// files and needs a rasteriser to observe. These two cases are that
// observation, and they are the reason the file is worth the device.

TEST_CASE("CONTRACT: a viewport offsets the pane and scales the whole of it")
{
	Harness harness;

	DrawList list = harness.begin();

	// Deliberately not symmetric in y. A pane at (32, 8) 32x24 sits at rows
	// 8..31 measured from the top and rows 32..55 measured from the bottom, so
	// a backend that forgot GL's origin flip - or applied it twice - lands
	// somewhere these assertions can tell apart. A centred pane could not.
	list.set_viewport(Viewport(32.0f, 8.0f, 32.0f, 24.0f));
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 32.0f, 24.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);
	harness.end();

	// The pane is filled corner to corner. THE FAR CORNER IS THE ASSERTION
	// THAT MATTERS: a backend that offset the rasteriser correctly but built
	// its pixels-to-clip transform from the back buffer instead of the pane
	// would put this sprite in the top left quarter of the pane and leave
	// (63, 31) black. That is exactly the shape of the bug the GL backend had.
	CHECK(harness.at(32, 8) == WHITE);
	CHECK(harness.at(63, 31) == WHITE);
	CHECK(harness.at(47, 20) == WHITE);

	// And nothing outside it. A viewport confines; it does not merely offset.
	CHECK(harness.at(31, 8) == BLACK);
	CHECK(harness.at(32, 7) == BLACK);
	CHECK(harness.at(32, 32) == BLACK);
	CHECK(harness.at(0, 0) == BLACK);
}

TEST_CASE("CONTRACT: two panes splitting a fraction cover every row between them")
{
	Harness harness;

	DrawList list = harness.begin();

	// 64 rows split at 31.5, which is what an odd client height does to a
	// two-player layout and what no integral test can produce. Each edge
	// truncates and the size is the difference (Viewport::pixel_rect), so the
	// panes come out 31 rows and 33 rows and meet exactly at row 31.
	//
	// TRUNCATING THE POSITION AND THE SIZE SEPARATELY - which is what the GL
	// backend used to do, inline, in its own file - gives 31 rows and 32 rows.
	// Those cover rows 0..30 and 31..62, and row 63 belongs to neither.
	list.set_viewport(Viewport(0.0f, 0.0f, 64.0f, 31.5f));
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 64.0f, 31.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);

	list.set_viewport(Viewport(0.0f, 31.5f, 64.0f, 32.5f));
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 64.0f, 33.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);

	harness.end();

	// EVERY ROW, stated as a loop rather than as three sampled rows, because
	// the failure this exists to catch is a single row of clear colour and
	// sampling is how it survived this long.
	for (int y = 0; y < BUFFER_SIZE; y++)
	{
		CHECK_MESSAGE(harness.at(32, y) == WHITE,
			"row ", y, " is not covered by either pane");
	}

	// Changing the viewport mid-list is legal and costs a flush, so the two
	// panes have to be two runs. If the second set_viewport had not flushed,
	// the first pane's sprite would have been rasterised under the second
	// pane's transform and the top of the frame would be black.
	CHECK(harness.at(0, 0) == WHITE);
	CHECK(harness.at(63, 63) == WHITE);
}

// THE FOUR CASES BELOW ARE THE FIRST MULTI-VIEW ANYTHING ON A RASTERISER.
//
// Every other case in this file fills one view, and the harness could not do
// otherwise until its capacity went from 1 to 4 - so the machinery the backends
// share least had until then been exercised by the null backend's recording
// tests and by nothing else. D3D11 gives each view a deferred context and binds
// a render target and a viewport into it, then finishes and executes one
// command list per view; D3D12 gives each view a command list and an allocator
// per frame in flight and hands the finished lists to its queue as one array;
// GL gives each view a vector of runs and replays them. Those are not three
// spellings of one thing, and view order, view isolation and what a view starts
// a frame holding are decided separately in each. What makes these worth
// writing now rather than then is the golden set:
// a multi-view frame is one more image, so agreeing about split-screen is
// checked at the same resolution as agreeing about one sprite.

TEST_CASE("CONTRACT: views draw in view order, whatever order they were filled")
{
	Harness harness;

	// Filled 1 then 0, submitted 0 then 1. The two orders are opposite on
	// purpose: everywhere else in this file they are the same order, so a
	// backend that replayed in the order the calls arrived would pass every
	// other case here and lose split-screen's layering entirely.
	DrawList zero = harness.begin(2);
	DrawList one = harness.view(1);

	one.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 64.0f, 64.0f), Colour(1.0f, 0.0f, 0.0f, 1.0f),
		0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);
	zero.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 64.0f, 64.0f), Colour(0.0f, 0.0f, 1.0f, 1.0f),
		0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);

	harness.end();

	// View 1 went last and is opaque, so view 1 is the whole frame. Call order
	// would have left this blue.
	CHECK(harness.at(0, 0) == RED);
	CHECK(harness.at(32, 32) == RED);
	CHECK(harness.at(63, 63) == RED);
}

TEST_CASE("CONTRACT: view state belongs to its view and not to the frame")
{
	Harness harness;

	// Both subcases put view 0's state somewhere that would MOVE view 1's
	// sprite if it leaked, rather than somewhere that would merely change it.
	// A leak that displaced a pane by nothing is the leak that survives.
	SUBCASE("a pane set on one view does not confine its neighbour")
	{
		DrawList zero = harness.begin(2);
		zero.set_viewport(Viewport(0.0f, 0.0f, 32.0f, 64.0f));
		zero.draw_sprite(harness.quad, Harness::white_texel(),
			RectangleF(0.0f, 0.0f, 32.0f, 64.0f),
			Colour(1.0f, 0.0f, 0.0f, 1.0f), 0.0f, Vector2F::ZERO,
			SpriteFlip::none, 0.0f);

		// No set_viewport at all, so this view's pane is the whole buffer and
		// its sprite is the far half of it. Under view 0's pane the same
		// destination is off the right-hand edge of a 32-wide view and clips
		// away to nothing.
		DrawList one = harness.view(1);
		one.draw_sprite(harness.quad, Harness::white_texel(),
			RectangleF(32.0f, 0.0f, 32.0f, 64.0f),
			Colour(0.0f, 0.0f, 1.0f, 1.0f), 0.0f, Vector2F::ZERO,
			SpriteFlip::none, 0.0f);

		harness.end();

		CHECK(harness.at(16, 32) == RED);
		CHECK(harness.at(48, 32) == BLUE);
	}

	SUBCASE("a camera set on one view does not move its neighbour")
	{
		// view = (world - translation) * scale, so view 0's sprite is at world
		// x 32 and lands at the left of the frame.
		DrawList zero = harness.begin(2);
		zero.set_camera(Camera(32.0f, 0.0f, 1.0f));
		zero.draw_sprite(harness.quad, Harness::white_texel(),
			RectangleF(32.0f, 0.0f, 32.0f, 64.0f),
			Colour(1.0f, 0.0f, 0.0f, 1.0f), 0.0f, Vector2F::ZERO,
			SpriteFlip::none, 0.0f);

		// The same rectangle, no camera, so it stays where it was written. A
		// leaked camera would put it on top of view 0's instead.
		DrawList one = harness.view(1);
		one.draw_sprite(harness.quad, Harness::white_texel(),
			RectangleF(32.0f, 0.0f, 32.0f, 64.0f),
			Colour(0.0f, 0.0f, 1.0f, 1.0f), 0.0f, Vector2F::ZERO,
			SpriteFlip::none, 0.0f);

		harness.end();

		CHECK(harness.at(16, 32) == RED);
		CHECK(harness.at(48, 32) == BLUE);
	}
}

TEST_CASE("CONTRACT: a view starts every frame holding nothing")
{
	Harness harness;

	// A pane in the corner and a camera off the origin, both on view 0.
	DrawList first = harness.begin();
	first.set_viewport(Viewport(32.0f, 0.0f, 32.0f, 64.0f));
	first.set_camera(Camera(8.0f, 8.0f, 1.0f));
	first.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(8.0f, 8.0f, 32.0f, 64.0f), Colour(1.0f, 0.0f, 0.0f, 1.0f),
		0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);
	harness.end();

	// The same view index, the next frame, and it sets neither. The three
	// backends reset in three different places - begin_frame on two of them,
	// set_view_count's rebind on the third - so this is the term that says the
	// three arrive at the same state rather than at three defensible ones.
	DrawList second = harness.begin();
	second.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 64.0f, 64.0f), Colour(0.0f, 0.0f, 1.0f, 1.0f),
		0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);
	harness.end();

	// Corner to corner. A pane that survived would leave the left half black;
	// a camera that survived would leave the far corner black, because the
	// sprite would start eight pixels up and to the left of where it says.
	CHECK(harness.at(0, 0) == BLUE);
	CHECK(harness.at(32, 32) == BLUE);
	CHECK(harness.at(63, 63) == BLUE);
}

TEST_CASE("CONTRACT: declaring another view leaves the filled ones alone")
{
	Harness harness;

	// A pane that is NOT where its sprite is written, which is what makes this
	// case able to see the failure at all: the destination is the left half of
	// a pane that sits in the right half of the buffer, so a view whose pane
	// was quietly put back to fullscreen draws in the wrong half rather than
	// in the same half by coincidence.
	DrawList zero = harness.begin();
	zero.set_viewport(Viewport(32.0f, 0.0f, 32.0f, 64.0f));
	zero.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 32.0f, 64.0f), Colour(1.0f, 0.0f, 0.0f, 1.0f),
		0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);

	// Raising the count mid-frame is legal and a scene does it whenever a
	// second player joins a running match. On D3D11 this is the call that
	// binds a render target and the screen viewport into a view's deferred
	// context, and it must bind the NEW one only.
	harness.renderer().set_view_count(2);

	DrawList one = harness.view(1);
	one.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 32.0f, 64.0f), Colour(0.0f, 0.0f, 1.0f, 1.0f),
		0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);

	harness.end();

	// View 0 in the right half under its own pane, view 1 in the left half
	// under the whole buffer. A rebind would have put both in the left half
	// and view 1 replays second, so the right half would be black.
	CHECK(harness.at(48, 32) == RED);
	CHECK(harness.at(16, 32) == BLUE);
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
		RectangleF(0.0f, 0.0f, static_cast<float>(BUFFER_SIZE),
			static_cast<float>(BUFFER_SIZE)),
		Colour::white, 0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);
	abandoned.draw_text(harness.font, L"AAAA", Vector2F(4.0f, 4.0f),
		Colour::white, 1.0f, 0.0f, Vector2F::ZERO, 0.0f);

	// AND NO end(), which is the whole case. A client reaches this state by
	// letting an exception out of its draw walk and carrying on; both samples
	// exit instead, so nothing in the tree reaches it, which is why the
	// backends were free to disagree about it. See
	// docs/review/backend-equivalence/README.md, defect B.

	std::ignore = harness.begin();
	harness.end();

	// The next frame is the clear and nothing else. Not sampled - the stranded
	// geometry is a full-screen quad, but the failure it stands for is any
	// already-flushed run arriving at the head of the next frame's commands.
	CHECK(harness.at(0, 0) == BLACK);
	CHECK(harness.ink_bounds() == RectangleI::ZERO);
}

TEST_CASE("CONTRACT: read_back_buffer hands back exactly back_buffer_size")
{
	Harness harness;

	// back_buffer_size is called by nothing else in this file and by nothing
	// in tests/ at all, so until now the seam's "exactly width * height * 4
	// bytes" was a sentence and not an assertion.
	std::ignore = harness.begin();
	harness.end();

	CHECK(harness.buffer_size() ==
		Vector2F(static_cast<float>(BUFFER_SIZE),
			static_cast<float>(BUFFER_SIZE)));
	CHECK(harness.byte_count() ==
		static_cast<size_t>(BUFFER_SIZE) * BUFFER_SIZE * 4);

	SUBCASE("and it still does when the window changed and nobody said so")
	{
		// THE ONE STATE IN WHICH A BACKEND CAN HOLD TWO ANSWERS TO "how big is
		// it". D3D11 draws into a swap chain it made at the size it was told,
		// so the window moving under it changes nothing here and the stretch
		// happens at Present. A WGL context has no such buffer - its default
		// framebuffer is the window's client area - so the GL backend has to
		// read the window, and it used to answer this from a cached int that
		// only create_device and window_size_changed ever wrote.
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
		harness.resize_window(BUFFER_SIZE, BUFFER_SIZE - 16);

		DrawList list = harness.begin();
		const Vector2F pane = harness.buffer_size();
		list.set_viewport(Viewport(0.0f, 0.0f, pane.x, pane.y));
		list.draw_sprite(harness.quad, Harness::white_texel(),
			RectangleF(0.0f, 0.0f, pane.x, pane.y), Colour::white, 0.0f,
			Vector2F::ZERO, SpriteFlip::none, 0.0f);
		// The one frame here that is not held against a golden image, because
		// the buffer it was read out of is a different size on each backend and
		// the seam says it must be. Harness::end_not_comparable explains.
		harness.end_not_comparable();

		const Vector2F buffer = harness.buffer_size();
		CHECK(harness.byte_count() == static_cast<size_t>(buffer.x) *
			static_cast<size_t>(buffer.y) * 4);

		// And a pane the size of that buffer covers it corner to corner, which
		// is the placement half of the same statement.
		CHECK(harness.at(0, 0) == WHITE);
		CHECK(harness.at(static_cast<int>(buffer.x) - 1,
			static_cast<int>(buffer.y) - 1) == WHITE);
	}
}

TEST_CASE("CONTRACT: the destination factor is INV_SRC_ALPHA, not ZERO")
{
	Harness harness;

	DrawList list = harness.begin();

	// An opaque white ground first. THE CASE ABOVE CANNOT DO THIS. It composites
	// its one sprite over the cleared frame, which is black - and black times
	// any destination factor is black, so ZERO and INV_SRC_ALPHA give the same
	// answer there and the RGB destination factor is undistinguished by it. The
	// alpha channel is pinned by that case; the colour channels were not pinned
	// by anything.
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 16.0f, 16.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);

	// Then a half-alpha black scrim over it. Premultiplied, which for black
	// means the colour channels are zero and stay zero however the alpha moves
	// - so the source contributes nothing at all and every channel of the
	// result comes from the destination through the factor under test. A tint
	// of (1,1,1,0.5) would NOT do: it is not premultiplied, and the source term
	// alone saturates the result to white.
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 16.0f, 16.0f), Colour(0.0f, 0.0f, 0.0f, 0.5f),
		0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);

	harness.end();

	const Pixel scrimmed = harness.at(8, 8);

	// dst * (1 - 0.5) = mid grey. ZERO gives black and ONE gives white, so a
	// range that excludes both settles the factor without pinning a rounding
	// mode - 127 and 128 are both defensible for 0.5 * 255 and a driver is
	// entitled to either.
	CHECK(scrimmed.r > 100);
	CHECK(scrimmed.r < 155);
	CHECK(scrimmed.g == scrimmed.r);
	CHECK(scrimmed.b == scrimmed.r);

	// And the alpha is the term the case above already pins, restated here
	// because a factor change that moved both would otherwise pass one test
	// and fail neither.
	CHECK(scrimmed.a == 255);

	// A scrim is what samples/linesweeper draws over the board when the game
	// is paused, so this is live behaviour and not a hypothetical.
}

TEST_CASE("CONTRACT: a resize arriving mid-frame restarts the frame")
{
	// THE ONE CASE HERE THAT IS NOT ABOUT A PIXEL, and it is in this file
	// because it is the only one with a device. renderer.h makes it a term of
	// the seam that window_size_changed may arrive between begin_frame and
	// submit - a resize reaches the shell as a window message, and a window
	// message can be delivered while the shell is inside a frame, so no rule
	// forbidding it would be one the caller could keep.
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
		Vector2F::ZERO, SpriteFlip::none, 0.0f);

	// Here, with a draw recorded and the list still in the caller's hand.
	//
	// THE WINDOW IS RESIZED TOO, AND NOT ONLY THE RENDERER TOLD, because
	// otherwise this case would be asking two backends a different question
	// from the third. back_buffer_size is answered from the swap chain on the
	// Direct3D backends and from the window on the OpenGL one - renderer.h
	// says so, and the drag-resize case above is built on the difference - so
	// telling the renderer about a resize the window never had would leave GL
	// drawing into 64x64 while the other two moved to 32x32. A real resize
	// moves both, in this order: the window changes, then the shell says so.
	harness.resize_window(BUFFER_SIZE / 2, BUFFER_SIZE / 2);
	CHECK(renderer.window_size_changed(BUFFER_SIZE / 2, BUFFER_SIZE / 2));

	// THE LIST THE CALLER IS HOLDING IS STILL A LIST. That is the half of the
	// rule a backend could satisfy by tearing everything down and still get
	// wrong: the frame is restarted, not abandoned, so this draw belongs to the
	// frame that is now running.
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);

	harness.end_not_comparable();

	// The buffer is the size the resize asked for, and read_back_buffer agreed
	// with it - which is what makes the assertions below addressable at all.
	CHECK(harness.buffer_size().x == static_cast<float>(BUFFER_SIZE / 2));
	CHECK(harness.buffer_size().y == static_cast<float>(BUFFER_SIZE / 2));

	// WHAT WAS DRAWN BEFORE THE RESIZE IS GONE, AND WHAT WAS DRAWN AFTER IT IS
	// THERE. The first sprite covered 32x32 of a 64x64 buffer and the second
	// covers 8x8 of a 32x32 one, so a pixel at (20, 20) is inside the first and
	// outside the second: it is the one place the two disagree, and it says
	// which frame the buffer is holding.
	CHECK(harness.at(4, 4) == WHITE);
	CHECK(harness.at(20, 20) == BLACK);
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
	// the case above gives: back_buffer_size is answered from the swap chain
	// on the Direct3D backends and from the window on the OpenGL one, so a
	// resize the window never had would ask two backends a different question
	// from the third.
	harness.resize_window(BUFFER_SIZE / 2, BUFFER_SIZE / 2);
	CHECK(renderer.window_size_changed(BUFFER_SIZE / 2, BUFFER_SIZE / 2));

	// And now the frame says what it has, which is the ordinary next call and
	// the one the resize arrived in front of.
	renderer.set_view_count(1);
	DrawList list = renderer.view(0);
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);

	harness.end_not_comparable();

	CHECK(harness.buffer_size().x == static_cast<float>(BUFFER_SIZE / 2));
	CHECK(harness.buffer_size().y == static_cast<float>(BUFFER_SIZE / 2));

	// The 8x8 sprite is there, and the buffer around it is the clear colour
	// rather than whatever ResizeBuffers left in it.
	CHECK(harness.at(4, 4) == WHITE);
	CHECK(harness.at(20, 20) == BLACK);
}

TEST_CASE("CONTRACT: re-loading a name reuses its slot, however many times")
{
	// "Re-adding a name reuses its slot" is registry.h's sentence and
	// render_resources.h repeats it, so a handle resolved before a re-load is
	// the same handle afterwards and draws the new bytes. What that has to mean
	// for a backend is that a re-load costs it nothing PER LOAD, and one of the
	// four was spending a fixed-size resource on every one: the D3D12
	// descriptor heap holds a fixed number of entries, its slot allocator is a
	// bump with no free list, and a re-load took a fresh slot and abandoned the
	// one the name already had. A client re-walking its manifest per level ran
	// that heap out with a few dozen textures live - and one manifest naming a
	// single asset as both a texture and a sprite sheet does it inside one
	// walk, because load_sprite_sheet loads the sheet's texture under the
	// sheet's own name.
	//
	// SO THE LOOP IS THE CASE, AND THE LAST DRAW IS THE ASSERTION. Three
	// hundred is comfortably past the smallest fixed capacity any backend in
	// this tree has, and a backend that leaks per load never reaches the draw:
	// the throw comes out of the load. The colour change on the final pass is
	// the other half - a slot that is reused and not rewritten would hand back
	// the first load's red.
	Harness harness;

	for (int load = 0; load < 299; load++)
	{
		add_texture_asset(harness.renderer(), harness.resources(), "reloaded",
			flat_texture(255, 0, 0));
	}
	add_texture_asset(harness.renderer(), harness.resources(), "reloaded",
		flat_texture(0, 255, 0));

	const TextureHandle reloaded =
		harness.resources().resolve_texture("reloaded");

	DrawList list = harness.begin();
	list.draw_sprite(reloaded, RectangleI(0, 0, 2, 2),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);
	harness.end();

	CHECK(harness.at(4, 4) == GREEN);
}
