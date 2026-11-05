#include <doctest/doctest.h>

#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/ui/widget.h"
#include "tests/render/null_synthetic_harness.h"

// A real UiText, drawn into a recording.
//
// COMPILED ONLY IN THE null CONFIGURATION, and the real-widget file
// stub_widget.h describes: the null backend supplies a Renderer with no device,
// so a UiText can be built and what it draws asserted rather than assumed.
// tests/ui/CMakeLists.txt adds it to UiTests under that backend alone, as
// tests/render/CMakeLists.txt does null_tests.cpp.
//
// WHAT IT PINS is the behaviour label.h points a reader at: scene text that
// must stop drawing while keeping its words, its box and its colour is a
// UiText, and set_hidden is how. Hidden, it draws nothing; its bounds(), its
// string and its colour are what they were.

namespace
{
	using namespace labrador;

	// A renderer with no graphics API, an atlas built without a file and a
	// two-glyph font over it - ten by ten, stepping ten, on a line twenty
	// tall, so "AB" is two sprites and measures twenty by twenty.
	class Harness
	{
	public:
		Harness() :
			null_(640, 480, 1)
		{
			this->null_.add_white_square("atlas", 16);
			this->null_.add_two_glyph_font("font", "atlas");
		}

		// How many sprites one draw of `object` records. A copy of the count,
		// because the recording is valid only until the next begin_frame.
		int sprites_drawn(const UiObject& object)
		{
			Renderer& renderer = this->null_.renderer();
			renderer.begin_frame();
			renderer.set_view_count(1);
			DrawList list = renderer.view(0);
			object.draw(list);
			renderer.submit();
			return static_cast<int>(this->null_.recording().size());
		}

		RenderResources& resources() { return this->null_.resources(); }

	private:
		NullSyntheticHarness null_;
	};
}

TEST_CASE("a hidden UiText draws nothing and keeps its words, box and colour")
{
	Harness harness;
	const Colour colour(0.25f, 0.5f, 0.75f, 1.0f);
	UiText text("label", L"AB", "font", Vector2F(100.0f, 50.0f),
		&harness.resources(), colour);

	const RectangleF shown_bounds = text.bounds();
	REQUIRE(shown_bounds.width > 0.0f);
	CHECK(harness.sprites_drawn(text) == 2);

	text.set_hidden(true);
	CHECK(text.hidden());
	CHECK(harness.sprites_drawn(text) == 0);
	CHECK(text.bounds() == shown_bounds);
	CHECK(text.text() == L"AB");
	CHECK(text.colour() == colour);

	text.set_hidden(false);
	CHECK(harness.sprites_drawn(text) == 2);
	CHECK(text.bounds() == shown_bounds);
}

TEST_CASE("a UiText constructed hidden draws nothing and still has its box")
{
	Harness harness;
	const UiText text("label", L"AB", "font", Vector2F(0.0f, 0.0f),
		&harness.resources(), Colour::white, true);

	CHECK(harness.sprites_drawn(text) == 0);
	CHECK(text.bounds() == RectangleF(0.0f, 0.0f, 20.0f, 20.0f));
}
