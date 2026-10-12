#include <doctest/doctest.h>

#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/font.h"
#include "engine/render/render_resources.h"
#include "engine/render/text.h"
#include "engine/render/text_drop_shadow.h"

#include <memory>
#include <string>
#include <vector>

// What a Text says and what colour it is, read back by its owner.
//
// Text publishes these two readers beside its setters so that anything built
// from it - a widget, a settings row - can be asked what it is showing. The
// cases below pin that each reader follows its setter, and that a drop shadow
// inherits both without its shadow colour leaking into either.

namespace
{
	using labrador::Colour;
	using labrador::Font;
	using labrador::Glyph;
	using labrador::RenderResources;
	using labrador::Text;
	using labrador::TextDropShadow;
	using labrador::TextureHandle;
	using mattmath::RectangleI;
	using mattmath::Vector2F;

	// Two cells on a line, which is all a string here needs: nothing below
	// measures, so the glyphs only have to exist.
	class Content
	{
	public:
		Content()
		{
			std::vector<Glyph> glyphs;
			for (char32_t character : { U'A', U'B' })
			{
				Glyph glyph;
				glyph.character = character;
				glyph.subrect = RectangleI(0, 0, 10, 10);
				glyphs.push_back(glyph);
			}
			this->resources.add_font("font",
				std::make_unique<Font>(TextureHandle(), std::move(glyphs),
					20.0f));
		}

		RenderResources resources;
	};
}

TEST_CASE("a text reads back the string and colour it was built with")
{
	Content content;
	const Text text(L"AB", "font", Vector2F(10.0f, 20.0f), &content.resources,
		Colour::red);

	CHECK(text.text() == L"AB");
	CHECK(text.colour() == Colour::red);
}

TEST_CASE("a text's readers follow its setters")
{
	Content content;
	Text text(L"AB", "font", Vector2F::ZERO, &content.resources);

	CHECK(text.colour() == Colour::white);

	text.set_text(L"BA");
	text.set_colour(Colour::blue);

	CHECK(text.text() == L"BA");
	CHECK(text.colour() == Colour::blue);
}

TEST_CASE("a drop shadow reads back its text's colour, not its shadow's")
{
	Content content;
	TextDropShadow shadow(L"AB", "font", Vector2F::ZERO, &content.resources,
		Colour::yellow, Colour::black);

	CHECK(shadow.text() == L"AB");
	CHECK(shadow.colour() == Colour::yellow);

	shadow.set_colour(Colour::green);
	CHECK(shadow.colour() == Colour::green);
	CHECK(shadow.shadow_color() == Colour::black);
}
