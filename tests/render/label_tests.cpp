#include <doctest/doctest.h>

#include "engine/core/game_object.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/font.h"
#include "engine/render/label.h"
#include "engine/render/render_resources.h"
#include "engine/render/sprite_geometry.h"
#include "engine/render/text.h"

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
		explicit Content(float a_y_offset = 0.0f) : resources()
		{
			std::vector<Glyph> glyphs;
			Glyph a = cell(U'A', 10, 10);
			a.y_offset = a_y_offset;
			glyphs.push_back(a);
			glyphs.push_back(cell(U'B', 10, 10));
			this->resources.add_font("font",
				std::make_unique<Font>(TextureHandle(), std::move(glyphs),
					20.0f));
		}

		RenderResources resources;
	};

	class FontSelectableText : public labrador::Text
	{
	public:
		using labrador::Text::Text;
		using labrador::TextObject::set_font;
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

TEST_CASE("negative glyph bearings expand cull bounds without changing layout")
{
	Content content(-4.0f);
	Label label(L"A", "font", Vector2F(10.0f, 92.0f), &content.resources);
	const labrador::FontHandle font = content.resources.resolve_sprite_font("font");

	CHECK(content.resources.measure_text(font, L"A") == Vector2F(10.0f, 20.0f));
	CHECK(label.bounds() == RectangleF(10.0f, 88.0f, 10.0f, 24.0f));

	// A string change must remove the previous string's overhang too.
	label.set_text(L"B");
	CHECK(label.bounds() == RectangleF(10.0f, 92.0f, 10.0f, 20.0f));
	label.set_text(L"");
	CHECK(label.bounds() == RectangleF(10.0f, 92.0f, 0.0f, 0.0f));
}

TEST_CASE("changing a font remeasures its glyph envelope")
{
	Content content(-4.0f);
	Glyph raised = cell(U'A', 10, 10);
	raised.y_offset = -12.0f;
	std::vector<Glyph> glyphs;
	glyphs.push_back(raised);
	content.resources.add_font("raised",
		std::make_unique<Font>(TextureHandle(), std::move(glyphs), 20.0f));
	FontSelectableText text(L"A", "font", Vector2F(10.0f, 92.0f),
		&content.resources);

	text.set_font("raised");
	CHECK(text.text_bounds() == RectangleF(10.0f, 80.0f, 10.0f, 32.0f));
	text.set_text(L"\nA");
	CHECK(text.text_bounds() == RectangleF(10.0f, 92.0f, 10.0f, 40.0f));
	text.set_text(L"A");
	text.set_font("font");
	CHECK(text.text_bounds() == RectangleF(10.0f, 88.0f, 10.0f, 24.0f));
}

TEST_CASE("glyph envelopes keep the layout origin through scaling and rotation")
{
	Content content(-4.0f);
	const Font* font = content.resources.font(
		content.resources.resolve_sprite_font("font"));
	const Vector2F position(100.0f, 200.0f);
	const Vector2F other_position(-20.0f, 15.0f);
	for (float rotation : { 0.0f, 1.57079633f, -0.7f })
	for (float scale : { 1.0f, 2.0f })
	for (const Vector2F& origin : { Vector2F::ZERO, Vector2F(3.0f, 5.0f) })
	{
		const Label label(L"A\nB", "font", position, &content.resources,
			labrador::Colour::white, scale, rotation, origin);
		for (bool hypothetical : { false, true })
		{
			const Vector2F draw_position = hypothetical ? other_position : position;
			const float draw_scale = hypothetical ? 0.5f : scale;
			const RectangleF bounds = hypothetical
				? label.text_bounds_at(draw_position, draw_scale) : label.bounds();
			font->for_each_glyph(L"A\nB",
				[&bounds, &draw_position, draw_scale, rotation, &origin]
				(const Glyph& glyph, const Vector2F& pen)
				{
					labrador::SpriteVertex corners[4];
					labrador::build_glyph_quad(draw_position, draw_scale, glyph,
						pen, Vector2F(32.0f, 32.0f), labrador::Colour::white,
						rotation, origin, corners);
					for (const labrador::SpriteVertex& corner : corners)
					{
						CHECK(corner.position.x >= bounds.left() - 0.001f);
						CHECK(corner.position.x <= bounds.right() + 0.001f);
						CHECK(corner.position.y >= bounds.top() - 0.001f);
						CHECK(corner.position.y <= bounds.bottom() + 0.001f);
					}
				});
		}
	}
}

TEST_CASE("drawable whitespace can extend below layout while skipped whitespace does not")
{
	for (bool drawable : { false, true })
	{
		RenderResources resources;
		Glyph space = cell(U' ', drawable ? 4 : 1, drawable ? 8 : 1);
		space.y_offset = drawable ? 3.0f : -50.0f;
		std::vector<Glyph> glyphs;
		glyphs.push_back(space);
		resources.add_font("spaces",
			std::make_unique<Font>(TextureHandle(), std::move(glyphs), 4.0f));
		const labrador::FontHandle font = resources.resolve_sprite_font("spaces");
		const Label label(L" ", "spaces", Vector2F(10.0f, -6.0f), &resources);
		if (drawable)
		{
			CHECK(resources.measure_text(font, L" ") == Vector2F(4.0f, 4.0f));
			CHECK(label.bounds() == RectangleF(10.0f, -6.0f, 4.0f, 11.0f));
		}
		else
		{
			CHECK(resources.measure_text(font, L" ") == Vector2F::ZERO);
			CHECK(label.bounds() == RectangleF(10.0f, -6.0f, 0.0f, 0.0f));
		}
	}
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
	// The unscaled envelope stays as it was; text_bounds_at multiplies it by
	// the scale on the way out. So a scale change costs no
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

TEST_CASE("a turned label reports the box around the turned string")
{
	// A half turn about (100, 20) of a 20x20 string puts every glyph up and
	// left of the position, across x=80..100 - and this is what a scene
	// culls against, so the unrotated box the old answer gave had the label
	// disappear from any view that ended at x=90 while it was drawn inside
	// it (docs/review/gpt6/README.md, G6-04). The arithmetic is
	// sprite_geometry.h's and is held to the glyph quads there; this pins
	// that a Label hands it on through the base a scene holds.
	Content content;
	const float HALF_TURN = 3.14159265f;
	const Label label(L"AB", "font", Vector2F(100.0f, 20.0f),
		&content.resources, labrador::Colour::white, 1.0f, HALF_TURN);

	const GameObject& object = label;
	const RectangleF box = object.bounds();
	CHECK(box.x == doctest::Approx(80.0f).epsilon(0.001));
	CHECK(box.y == doctest::Approx(0.0f).epsilon(0.001));
	CHECK(box.width == doctest::Approx(20.0f).epsilon(0.001));
	CHECK(box.height == doctest::Approx(20.0f).epsilon(0.001));
}
