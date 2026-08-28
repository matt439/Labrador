#include <doctest/doctest.h>

#include "engine/math/rectanglef.h"
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

// The shadow's terms, and the one relationship no contract describes.
//
// WHY THIS FILE EXISTS. TextDropShadow was one of the twelve .cpp under
// engine/render/ that no test named, and the 2026-08-19 audit singled it out
// from the other eleven: GAPS.md item 8 called it one of three carrying pixel
// arithmetic that reaches the seam, because draw() makes TWO draw_text calls
// at a separate offset and scale - "a relationship no contract term
// describes".
//
// This file describes it. The two calls themselves need a DrawList, so the
// ordering and the offset arithmetic belong with the null backend's recording;
// what is stated here is the half that needs no device and is the half a
// caller gets wrong: THE SHADOW'S SCALE AND OFFSET ARE INDEPENDENT OF THE
// TEXT'S. Scaling the text does not scale either of them, so a label that
// looks right at 1.0 has a shadow sitting in the wrong place at 4.0, and
// nothing anywhere says so. That is a design decision rather than a defect -
// the shadow is a fixed screen-space offset, which is what a drop shadow
// usually wants - but it is a decision no reader could have found.
//
// The second thing here is the measured box, and it is the same shape of
// omission: text_bounds() is TextObject's and knows nothing about the shadow,
// so the extent a scene culls against excludes the shadow entirely.

namespace
{
	using labrador::Colour;
	using labrador::Font;
	using labrador::Glyph;
	using labrador::RenderResources;
	using labrador::Text;
	using labrador::TextDropShadow;
	using labrador::TextureHandle;
	using mattmath::RectangleF;
	using mattmath::RectangleI;
	using mattmath::Vector2F;

	// The same shape font_tests.cpp invents for the same reason: a cell whose
	// step is exactly its width, so a measurement is a round number rather than
	// a property of whatever .spritefont happened to be on disk.
	Glyph cell(char32_t character, int width, int height)
	{
		Glyph glyph;
		glyph.character = character;
		glyph.subrect = RectangleI(0, 0, width, height);
		return glyph;
	}

	// Ten by ten, stepping ten, on a line twenty tall - so "AB" is exactly
	// twenty by twenty and every number below is exact under /fp:precise.
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

TEST_CASE("a drop shadow starts two texels down and to the right, in black")
{
	const TextDropShadow shadow;

	// The defaults are on the header rather than in a document, and they are
	// the whole of what a caller gets who names none of them.
	CHECK(shadow.shadow_offset() == Vector2F(2.0f, 2.0f));
	CHECK(shadow.shadow_color() == Colour::black);
	CHECK(shadow.shadow_scale() == 1.0f);
}

TEST_CASE("the shadow's three terms are the constructor's, when it names them")
{
	Content content;
	const TextDropShadow shadow(L"AB", "font", Vector2F(100.0f, 50.0f),
		&content.resources, Colour::white, Colour::red,
		Vector2F(-3.0f, 5.0f), 1.0f, 0.5f);

	// A negative offset and a shadow smaller than the text, because both are
	// legal and neither is the default - the point is that the constructor
	// stores what it was handed rather than clamping it into a shape.
	CHECK(shadow.shadow_offset() == Vector2F(-3.0f, 5.0f));
	CHECK(shadow.shadow_color() == Colour::red);
	CHECK(shadow.shadow_scale() == 0.5f);
}

TEST_CASE("the shadow's three setters store what they are given")
{
	TextDropShadow shadow;

	shadow.set_shadow_offset(Vector2F(8.0f, -1.5f));
	shadow.set_shadow_color(Colour::blue);
	shadow.set_shadow_scale(2.0f);

	CHECK(shadow.shadow_offset() == Vector2F(8.0f, -1.5f));
	CHECK(shadow.shadow_color() == Colour::blue);
	CHECK(shadow.shadow_scale() == 2.0f);
}

TEST_CASE("scaling the text moves neither the shadow's scale nor its offset")
{
	Content content;
	TextDropShadow shadow(L"AB", "font", Vector2F(100.0f, 50.0f),
		&content.resources);

	shadow.set_scale(4.0f);

	// THE RELATIONSHIP THE AUDIT SAID NOTHING DESCRIBED. The text is four times
	// the size and the shadow is still one times it, still two texels away -
	// so at 4.0 the shadow sits a quarter of the way under a letter where at
	// 1.0 it sat clear of it, and the offset that was chosen against the small
	// version is wrong for the large one.
	//
	// The two calls in draw() are what make this true: the shadow is drawn with
	// shadow_scale_ and position() + shadow_offset_, neither of which consults
	// scale(). Deliberate - a drop shadow is usually a fixed screen-space
	// offset - and stated here because a caller who scales text and expects the
	// shadow to follow gets no diagnostic at all.
	CHECK(shadow.shadow_scale() == 1.0f);
	CHECK(shadow.shadow_offset() == Vector2F(2.0f, 2.0f));
}

TEST_CASE("the measured box is the text's, and the shadow is outside it")
{
	Content content;
	const TextDropShadow shadow(L"AB", "font", Vector2F(100.0f, 50.0f),
		&content.resources);
	const Text plain(L"AB", "font", Vector2F(100.0f, 50.0f),
		&content.resources);

	// Two ten-wide cells on a twenty-tall line, at (100, 50).
	const RectangleF measured(100.0f, 50.0f, 20.0f, 20.0f);
	CHECK(shadow.text_bounds() == measured);

	// And it is the same box the plain Text reports, which is the statement:
	// text_bounds() is TextObject's, it measures the string in the font, and no
	// override anywhere widens it by the shadow. So the shadow's own quad
	// starts at (102, 52) and runs to (122, 72) - two texels outside the extent
	// on each of two sides - and anything culling or laying out against
	// bounds() is working with a box the drawing overflows.
	CHECK(plain.text_bounds() == shadow.text_bounds());
}
