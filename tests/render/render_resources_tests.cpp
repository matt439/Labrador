#include <doctest/doctest.h>

#include "engine/render/render_resources.h"

#include <stdexcept>

using artattack::FontHandle;
using artattack::RenderResources;

TEST_CASE("the font queries read through the same handle contract as the rest")
{
	// Constructible with no device, which is what the pimpl seam bought and
	// what makes this the only part of the font path testable headlessly. The
	// substitution itself - a font that draws a question mark for a glyph it
	// has not got - needs a real SpriteFont, and a SpriteFont needs a device.
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
