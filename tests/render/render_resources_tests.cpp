#include <doctest/doctest.h>

#include "engine/render/render_resources.h"

#include <stdexcept>

using artattack::FontHandle;
using artattack::RenderResources;

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
