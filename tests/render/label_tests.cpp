#include <doctest/doctest.h>

#include "engine/core/game_object.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/font.h"
#include "engine/render/label.h"
#include "engine/render/render_resources.h"

#include <memory>
#include <string>
#include <vector>

// The string a scene can hold.
//
// WHY THIS FILE EXISTS. Label was one of the twelve .cpp under engine/render/
// that no test named. It is thirty-eight lines and three of its four methods
// are one-liners, which is exactly the shape that reads as not worth an
// assertion - and the one thing it adds over Text is the one thing worth
// stating: a GameObject base, so a container can hold a string next to a
// sprite and cull it. That is what the last case here says.
//
// WHAT IS PINNED AND WHAT IS NOT. draw() forwards to TextObject::draw and
// needs a DrawList, so it is not here. bounds() is the culling path and is
// arithmetic, so all of it is.

namespace
{
	using labrador::Font;
	using labrador::GameObject;
	using labrador::Glyph;
	using labrador::Label;
	using labrador::RenderResources;
	using labrador::TextureHandle;
	using mattmath::RectangleF;
	using mattmath::RectangleI;
	using mattmath::Vector2F;

	Glyph cell(char32_t character, int width, int height)
	{
		Glyph glyph;
		glyph.character = character;
		glyph.subrect = RectangleI(0, 0, width, height);
		return glyph;
	}

	// Ten by ten, stepping ten, on a line twenty tall - so "AB" measures
	// exactly twenty by twenty and every number below is exact.
	class Content
	{
	public:
		Content() : resources()
		{
			std::vector<Glyph> glyphs;
			glyphs.push_back(cell(U'A', 10, 10));
			glyphs.push_back(cell(U'B', 10, 10));
			this->resources.add_font("font",
				std::make_unique<Font>(TextureHandle(), std::move(glyphs),
					20.0f));
		}

		RenderResources resources;
	};
}

TEST_CASE("a label with no font and no string has an empty box")
{
	const Label label;

	// The default constructor resolves nothing and measures nothing -
	// render_resources_ stays null and remeasure() is never reached - so this
	// is also the statement that a default-constructed one is safe to ask.
	CHECK(label.bounds() == RectangleF::ZERO);
}

TEST_CASE("a label's box is the measured string, at the position")
{
	Content content;
	const Label label(L"AB", "font", Vector2F(100.0f, 50.0f),
		&content.resources);

	// Two ten-wide cells on a twenty-tall line. The height is the line
	// spacing rather than the glyph height, which is font.h's arithmetic and
	// is pinned there; what this says is that bounds() hands it back unaltered
	// and puts it at the position.
	CHECK(label.bounds() == RectangleF(100.0f, 50.0f, 20.0f, 20.0f));
}

TEST_CASE("changing the string remeasures the box")
{
	Content content;
	Label label(L"AB", "font", Vector2F(0.0f, 0.0f), &content.resources);

	CHECK(label.bounds() == RectangleF(0.0f, 0.0f, 20.0f, 20.0f));

	// set_text is one of the two setters that walk the string again, which is
	// the whole reason bounds() itself does not.
	label.set_text(L"A");
	CHECK(label.bounds() == RectangleF(0.0f, 0.0f, 10.0f, 20.0f));
}

TEST_CASE("moving a label moves its box and measures nothing")
{
	Content content;
	Label label(L"AB", "font", Vector2F(0.0f, 0.0f), &content.resources);

	label.set_position(Vector2F(-7.5f, 12.0f));

	CHECK(label.bounds() == RectangleF(-7.5f, 12.0f, 20.0f, 20.0f));
}

TEST_CASE("scaling a label scales the box it reports")
{
	Content content;
	Label label(L"AB", "font", Vector2F(100.0f, 50.0f), &content.resources);

	label.set_scale(2.0f);

	// THE MEASUREMENT IS NOT RETAKEN AND THE BOX IS STILL SCALED, which are
	// two different statements and only the first is what set_scale does.
	// measured_size_ is the unscaled walk and stays as it was; text_bounds_at
	// multiplies it by the scale on the way out. So a scale change costs no
	// string walk and still moves the extent a scene culls against.
	//
	// Worth being exact about, because label.h's own comment on bounds() says
	// "the box this hands back is the unscaled one whatever the scale is",
	// and this case is the reason to read that sentence as being about the
	// measurement rather than about the return value - the return value
	// scales.
	CHECK(label.bounds() == RectangleF(100.0f, 50.0f, 40.0f, 40.0f));
}

TEST_CASE("a label has nothing to step")
{
	Content content;
	Label label(L"AB", "font", Vector2F(100.0f, 50.0f), &content.resources);

	const RectangleF before = label.bounds();
	label.update(1.0f / 60.0f);
	label.update(1000.0f);

	// update() is empty and that is the design: a label that animates is a
	// label something else moves. Stated because an empty override is
	// indistinguishable from a forgotten one until somebody says which it is.
	CHECK(label.bounds() == before);
}

TEST_CASE("a label is a game object, which is the whole reason it exists")
{
	Content content;
	const Label label(L"AB", "font", Vector2F(100.0f, 50.0f),
		&content.resources);

	// Reached as the base, because that is how a Scene holds one: a
	// vector<unique_ptr<GameObject>> next to the sprites. Text on its own
	// cannot go in that vector, and this upcast is the difference between the
	// two classes.
	const GameObject& object = label;

	CHECK(object.bounds() == RectangleF(100.0f, 50.0f, 20.0f, 20.0f));
}
