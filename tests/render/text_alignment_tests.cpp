#include <doctest/doctest.h>

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/font.h"
#include "engine/render/sprite_font_file.h"
#include "engine/render/text_alignment.h"

#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace labrador;

TEST_SUITE("measured text alignment")
{
	TEST_CASE("all nine alignments place matching layout points in the anchor region")
	{
		const RectangleF anchor(10.25f, -20.5f, 40.0f, 30.0f);
		const Vector2F measured(7.0f, 5.0f);
		struct Placement
		{
			TextAlignment alignment;
			Vector2F position;
		};
		const std::array<Placement, 9> cases = { {
			{ TextAlignment::top_left, Vector2F(10.25f, -20.5f) },
			{ TextAlignment::top_centre, Vector2F(23.25f, -20.5f) },
			{ TextAlignment::top_right, Vector2F(36.25f, -20.5f) },
			{ TextAlignment::centre_left, Vector2F(10.25f, -10.5f) },
			{ TextAlignment::centre, Vector2F(23.25f, -10.5f) },
			{ TextAlignment::centre_right, Vector2F(36.25f, -10.5f) },
			{ TextAlignment::bottom_left, Vector2F(10.25f, -0.5f) },
			{ TextAlignment::bottom_centre, Vector2F(23.25f, -0.5f) },
			{ TextAlignment::bottom_right, Vector2F(36.25f, -0.5f) },
		} };
		for (const Placement& entry : cases)
		{
			CHECK(aligned_text_position(measured, anchor, entry.alignment,
				TextSnapping::none, 2.0f) == entry.position);
		}
	}

	TEST_CASE("point anchors preserve odd and fractional extents until snapping is selected")
	{
		const Vector2F size(7.5f, 5.0f);
		const Vector2F anchor(-0.25f, 10.75f);
		CHECK(aligned_text_position(size, anchor, TextAlignment::centre,
			TextSnapping::none) == Vector2F(-4.0f, 8.25f));
		CHECK(aligned_text_position(size, anchor, TextAlignment::centre,
			TextSnapping::floor) == Vector2F(-4.0f, 8.0f));
		CHECK(aligned_text_position(Vector2F(7.0f, 5.0f),
			Vector2F(-0.25f, -0.25f), TextAlignment::centre,
			TextSnapping::floor, 0.5f) == Vector2F(-2.0f, -2.0f));
	}

	TEST_CASE("empty and multiline font measurements align as one layout block")
	{
		Glyph a;
		a.character = U'A';
		a.subrect = RectangleI(0, 0, 3, 5);
		Glyph b = a;
		b.character = U'B';
		b.subrect.width = 5;
		const Font font(TextureHandle(), std::vector<Glyph>{ a, b }, 7.5f);
		const Vector2F anchor(20.25f, -5.75f);
		CHECK(font.measure(L"") == Vector2F::zero);
		CHECK(aligned_text_position(font.measure(L""), anchor,
			TextAlignment::centre, TextSnapping::none, 1.5f) == anchor);
		CHECK(aligned_text_position(font.measure(L""), anchor,
			TextAlignment::centre, TextSnapping::floor, 1.5f) ==
			Vector2F(20.0f, -6.0f));
		REQUIRE(font.measure(L"A\nBB") == Vector2F(10.0f, 15.0f));
		CHECK(aligned_text_position(font.measure(L"A\nBB"), anchor,
			TextAlignment::centre, TextSnapping::none, 1.5f) ==
			Vector2F(12.75f, -17.0f));
		CHECK(aligned_text_position(font.measure(L"A\nBB"), anchor,
			TextAlignment::bottom_right, TextSnapping::none, 1.5f) ==
			Vector2F(5.25f, -28.25f));
	}

	TEST_CASE("a loaded font's measured layout reaches its requested right and bottom anchor")
	{
		const SpriteFontFile data = read_sprite_font_file(
			"./content/jetbrains_mono_bold_16.spritefont");
		const Font font(TextureHandle(), data.glyphs, data.line_spacing);
		const Vector2F size = font.measure(L"A\nAAA");
		REQUIRE(size.x > 0.0f);
		REQUIRE(size.y > 0.0f);
		const Vector2F anchor(101.25f, -203.75f);
		const Vector2F position = aligned_text_position(size, anchor,
			TextAlignment::bottom_right, TextSnapping::none, 1.5f);
		CHECK(position.x + size.x * 1.5f == doctest::Approx(anchor.x));
		CHECK(position.y + size.y * 1.5f == doctest::Approx(anchor.y));
		CHECK(position.x != std::floor(position.x));
	}

	TEST_CASE("invalid inputs and unrepresentable results fail explicitly")
	{
		const Vector2F size(3.0f, 5.0f);
		const RectangleF anchor(0.0f, 0.0f, 10.0f, 20.0f);
		for (const float bad : { -1.0f, std::numeric_limits<float>::infinity(),
			std::numeric_limits<float>::quiet_NaN() })
		{
			CHECK_THROWS_AS(aligned_text_position(Vector2F(bad, 5.0f), anchor,
				TextAlignment::centre, TextSnapping::none), std::invalid_argument);
			CHECK_THROWS_AS(aligned_text_position(Vector2F(3.0f, bad), anchor,
				TextAlignment::centre, TextSnapping::none), std::invalid_argument);
			CHECK_THROWS_AS(aligned_text_position(size,
				RectangleF(0.0f, 0.0f, bad, 20.0f), TextAlignment::centre,
				TextSnapping::none), std::invalid_argument);
			CHECK_THROWS_AS(aligned_text_position(size,
				RectangleF(0.0f, 0.0f, 10.0f, bad), TextAlignment::centre,
				TextSnapping::none), std::invalid_argument);
			CHECK_THROWS_AS(aligned_text_position(size, anchor,
				TextAlignment::centre, TextSnapping::none, bad), std::invalid_argument);
		}
		CHECK_THROWS_AS(aligned_text_position(size, anchor,
			TextAlignment::centre, TextSnapping::none, 0.0f), std::invalid_argument);
		CHECK_THROWS_AS(aligned_text_position(size,
			Vector2F(std::numeric_limits<float>::infinity(), 0.0f),
			TextAlignment::centre, TextSnapping::none), std::invalid_argument);
		CHECK_THROWS_AS(aligned_text_position(size,
			Vector2F(0.0f, std::numeric_limits<float>::quiet_NaN()),
			TextAlignment::centre, TextSnapping::none), std::invalid_argument);
		CHECK_THROWS_AS(aligned_text_position(size, anchor,
			static_cast<TextAlignment>(-1), TextSnapping::none), std::invalid_argument);
		CHECK_THROWS_AS(aligned_text_position(size, anchor,
			TextAlignment::centre, static_cast<TextSnapping>(-1)), std::invalid_argument);
		// Exercise checked overflow at runtime, including under MSVC IPO.
		volatile float runtime_largest = std::numeric_limits<float>::max();
		const float largest = runtime_largest;
		CHECK_THROWS_AS(aligned_text_position(Vector2F(largest, 1.0f), anchor,
			TextAlignment::centre, TextSnapping::none, 2.0f), std::invalid_argument);
		CHECK_THROWS_AS(aligned_text_position(Vector2F(largest, 1.0f),
			Vector2F(-largest, 0.0f), TextAlignment::top_right,
			TextSnapping::none), std::invalid_argument);
	}
}
