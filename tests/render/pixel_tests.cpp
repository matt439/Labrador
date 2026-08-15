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

#include <ostream>
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
// filtering, and text of any kind. Text needs a glyph atlas and is the larger
// half of what a backend owes; these cover the sprite path alone.
//
// A HIDDEN WINDOW, not an offscreen target. create_device wants a window
// handle because a swap chain does, so the cheapest honest answer is a real
// window that is never shown. It adds nothing to the seam, and if an offscreen
// path is ever wanted for its own sake it can replace this without touching a
// single assertion below.

namespace
{
	using namespace artattack;
	using namespace mattmath;

	constexpr int BUFFER_SIZE = 64;

	// Registered once per process, on first use. Never shown: there is no
	// ShowWindow call anywhere in this file, so a test run puts nothing on
	// screen and steals no focus.
	HWND create_hidden_window()
	{
		static const wchar_t* CLASS_NAME = L"ArtAttackPixelTestWindow";

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

		// The whole 2x2 texture, and single texels out of it. Rectangles here
		// are (x, y, width, height), which is why a one-texel source is
		// (1, 1, 1, 1) and not (1, 1, 2, 2).
		static RectangleI whole() { return RectangleI(0, 0, 2, 2); }
		static RectangleI red_texel() { return RectangleI(0, 0, 1, 1); }
		static RectangleI white_texel() { return RectangleI(1, 1, 1, 1); }

		TextureHandle quad;

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
