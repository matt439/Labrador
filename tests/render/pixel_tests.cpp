#include "engine/render/renderer.h"
#include "engine/render/camera.h"
#include "engine/render/colour.h"
#include "engine/render/render_resources.h"
#include "engine/render/resource_factory.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"

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
// AND ONE MORE THAT IS NOT A GAP IN THE LIST BUT A PROPERTY OF THE METHOD.
// Because the text cases below are relationships (see two paragraphs down), a
// term added EQUALLY TO EVERY GLYPH cancels out of every one of them and this
// file cannot see it at all. The glyph's vertical bearing is exactly such a
// term: deleting it used to leave this file green on both real backends, the
// null backend's record test green, and the samples merely a few pixels wrong.
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

	// A device, a table with one texture in it, and a frame you can read back.
	class Harness
	{
	public:
		Harness()
		{
			this->window_ = create_hidden_window();
			this->renderer_.create_device(this->window_, BUFFER_SIZE,
				BUFFER_SIZE, 1);
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

		DrawList begin()
		{
			this->renderer_.begin_frame();
			this->renderer_.set_view_count(1);
			return this->renderer_.view(0);
		}

		// Deliberately no end_frame(). Presenting a FLIP_DISCARD swap chain
		// throws the back buffer's contents away, which is exactly what is
		// being read here. begin_frame clears and rebinds on the next pass, so
		// never presenting costs a test nothing.
		void end()
		{
			this->renderer_.submit();
			this->renderer_.read_back_buffer(this->pixels_);
		}

		Pixel at(int x, int y) const
		{
			const size_t offset =
				(static_cast<size_t>(y) * BUFFER_SIZE + static_cast<size_t>(x))
				* 4;
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
		HWND window_ = nullptr;
		Renderer renderer_;
		RenderResources resources_;
		std::vector<unsigned char> pixels_;
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
