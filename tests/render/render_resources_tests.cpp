#include <doctest/doctest.h>

#include "engine/core/name_table.h"
#include "engine/math/rectanglei.h"
#include "engine/render/animation_strip.h"
#include "engine/render/render_resources.h"
#include "engine/render/sprite_frame.h"
#include "engine/render/sprite_sheet.h"

#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using labrador::FontHandle;
using labrador::RenderResources;

TEST_CASE("the font queries read through the same handle contract as the rest")
{
	// Constructible with no device, which is what the pimpl seam bought. This
	// is the handle contract and nothing else: what a font ANSWERS is
	// arithmetic and is asserted in font_tests.cpp, which needs no table
	// either. What is left here is what only a table can get wrong.
	RenderResources resources;

	// An unresolved handle throws rather than reading slot zero, which is what
	// Handle defaults to invalid for. The two new queries join measure_text in
	// that rather than answering "cannot render": "I have no font" and "this
	// font cannot draw that" are different facts and only the second is about
	// the text.
	CHECK_THROWS_AS(resources.can_render(FontHandle(), L"text"),
		std::out_of_range);
	CHECK_THROWS_AS(resources.first_unrenderable(FontHandle(), L"text"),
		std::out_of_range);
	CHECK_THROWS_AS(resources.measure_text(FontHandle(), L"text"),
		std::out_of_range);

	// The empty string is not a shortcut past that. A caller checking every
	// string in a content file will hand this some empty ones, and getting an
	// answer for a font that does not exist would be the wrong answer rather
	// than a cheap one.
	CHECK_THROWS_AS(resources.first_unrenderable(FontHandle(), L""),
		std::out_of_range);

	CHECK_THROWS_AS(resources.resolve_sprite_font("never_loaded"),
		std::out_of_range);
}

namespace
{
	using labrador::AnimationStrip;
	using labrador::NameTable;
	using labrador::SpriteFrame;
	using labrador::SpriteSheet;
	using labrador::TextureHandle;
	using labrador::RectangleI;

	// Two sheets that both hold a frame and a strip called "a", in different
	// slots - so a pair resolved against the wrong sheet would name the
	// wrong element rather than fail, which is the mismatch the pair types
	// exist to make inexpressible.
	RenderResources sheets()
	{
		RenderResources resources;

		NameTable<SpriteFrame> first_frames("sprite frame");
		first_frames.add("a", SpriteFrame(RectangleI(0, 0, 4, 4)));
		first_frames.add("b", SpriteFrame(RectangleI(4, 0, 4, 4)));
		NameTable<AnimationStrip> first_strips("animation strip");
		first_strips.add("a",
			AnimationStrip(RectangleI(0, 0, 4, 4), 2, 0.25f, false));
		first_strips.add("b",
			AnimationStrip(RectangleI(0, 4, 4, 4), 2, 0.5f, false));
		resources.add_sprite_sheet("first",
			std::make_unique<SpriteSheet>(TextureHandle(),
				std::move(first_frames), std::move(first_strips)));

		NameTable<SpriteFrame> second_frames("sprite frame");
		second_frames.add("b", SpriteFrame(RectangleI(8, 0, 4, 4)));
		second_frames.add("a", SpriteFrame(RectangleI(12, 0, 4, 4)));
		NameTable<AnimationStrip> second_strips("animation strip");
		second_strips.add("b",
			AnimationStrip(RectangleI(0, 8, 4, 4), 2, 1.0f, false));
		second_strips.add("a",
			AnimationStrip(RectangleI(0, 12, 4, 4), 2, 2.0f, false));
		resources.add_sprite_sheet("second",
			std::make_unique<SpriteSheet>(TextureHandle(),
				std::move(second_frames), std::move(second_strips)));

		return resources;
	}
}

TEST_CASE("a sheet pair resolves to what resolving each half separately gives")
{
	const RenderResources resources = sheets();

	const RenderResources::SheetFrame frame =
		resources.resolve_sheet_frame("second", "a");
	CHECK(frame.sheet() == resources.resolve_sprite_sheet("second"));
	CHECK(frame.frame() ==
		resources.sprite_sheet("second")->resolve_sprite_frame("a"));

	const RenderResources::SheetStrip strip =
		resources.resolve_sheet_strip("second", "a");
	CHECK(strip.sheet() == resources.resolve_sprite_sheet("second"));
	CHECK(strip.strip() ==
		resources.sprite_sheet("second")->resolve_animation_strip("a"));

	// The same element name against the other sheet is another pair, and
	// the two compare unequal however the element slots happen to line up.
	CHECK(resources.resolve_sheet_frame("first", "a") != frame);
	CHECK(resources.resolve_sheet_frame("second", "a") == frame);
	CHECK(resources.resolve_sheet_strip("first", "a") != strip);
}

TEST_CASE("a sheet pair names whichever half it could not resolve")
{
	const RenderResources resources = sheets();

	// The message is what a content author reads while a level loads, so it
	// says which of the two names was wrong - and, for a missing element,
	// which sheet it was looked for in.
	CHECK_THROWS_WITH_AS(resources.resolve_sheet_frame("third", "a"),
		doctest::Contains("'third'"), std::out_of_range);
	CHECK_THROWS_WITH_AS(resources.resolve_sheet_frame("first", "c"),
		doctest::Contains("'c'"), std::out_of_range);
	CHECK_THROWS_WITH_AS(resources.resolve_sheet_frame("first", "c"),
		doctest::Contains("'first'"), std::out_of_range);

	CHECK_THROWS_WITH_AS(resources.resolve_sheet_strip("third", "a"),
		doctest::Contains("'third'"), std::out_of_range);
	CHECK_THROWS_WITH_AS(resources.resolve_sheet_strip("first", "c"),
		doctest::Contains("'c'"), std::out_of_range);
	CHECK_THROWS_WITH_AS(resources.resolve_sheet_strip("first", "c"),
		doctest::Contains("'first'"), std::out_of_range);
}

TEST_CASE("a default sheet pair is unresolved in both halves")
{
	const RenderResources::SheetFrame frame;
	const RenderResources::SheetStrip strip;

	CHECK_FALSE(frame.sheet().valid());
	CHECK_FALSE(frame.frame().valid());
	CHECK_FALSE(strip.sheet().valid());
	CHECK_FALSE(strip.strip().valid());
}

TEST_SUITE("FontWrapResourceTests")
{
	TEST_CASE("wrap_text resolves a font handle before text or width validation")
	{
		RenderResources resources;
		CHECK_THROWS_AS(resources.wrap_text(FontHandle(), L"A", 10.0f),
			std::out_of_range);
		CHECK_THROWS_AS(resources.wrap_text(FontHandle(), L"", 10.0f),
			std::out_of_range);
		CHECK_THROWS_AS(resources.wrap_text(FontHandle(), L"", 0.0f),
			std::out_of_range);
		CHECK_THROWS_AS(resources.wrap_text(FontHandle(), L"",
			std::numeric_limits<float>::quiet_NaN()), std::out_of_range);
	}

	TEST_CASE("wrap_text forwards through the current font occupying its slot")
	{
		labrador::Glyph letter;
		letter.character = U'A';
		letter.subrect = labrador::RectangleI(0, 0, 10, 10);
		labrador::Glyph space;
		space.character = U' ';
		space.subrect = labrador::RectangleI(0, 0, 1, 1);
		space.x_advance = 4.0f;
		RenderResources resources;
		resources.add_font("menu", std::make_unique<labrador::Font>(
			labrador::TextureHandle(), std::vector<labrador::Glyph>{ letter, space },
			20.0f));
		const FontHandle handle = resources.resolve_sprite_font("menu");
		CHECK(resources.wrap_text(handle, L"A A", 25.0f) == L"A A");
		CHECK(resources.wrap_text(handle, L"A A", 10.0f) == L"A\nA");
		CHECK(resources.wrap_text(handle, L"A A", 10.0f) ==
			resources.font(handle)->wrap(L"A A", 10.0f));
		const wchar_t bounded[] = { L'A', L' ', L'A', L'Z' };
		CHECK(resources.wrap_text(handle, std::wstring_view(bounded, 3), 10.0f)
			== L"A\nA");
		CHECK(resources.wrap_text(handle, L"", 10.0f).empty());
		CHECK_THROWS_AS(resources.wrap_text(handle, L"", 0.0f),
			std::invalid_argument);
		CHECK_THROWS_AS(resources.wrap_text(handle, L"Z", -1.0f),
			std::invalid_argument);
		CHECK_THROWS_WITH_AS(resources.measure_text(handle, L"Z"),
			doctest::Contains("U+005A"), std::runtime_error);
		CHECK_THROWS_WITH_AS(resources.wrap_text(handle, L"Z", 10.0f),
			doctest::Contains("U+005A"), std::runtime_error);
		CHECK_FALSE(resources.can_render(handle, L"Z"));
		letter.subrect.width = 4;
		resources.add_font("menu", std::make_unique<labrador::Font>(
			labrador::TextureHandle(), std::vector<labrador::Glyph>{ letter, space },
			20.0f));
		CHECK(resources.resolve_sprite_font("menu") == handle);
		CHECK(resources.wrap_text(handle, L"A A", 13.0f) == L"A A");
		CHECK(resources.wrap_text(handle, L"A A", 12.0f) == L"A\nA");
	}
}
