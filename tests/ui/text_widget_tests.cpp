#include <doctest/doctest.h>

#include "engine/render/null/recording.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/font.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"
#include "engine/ui/widget.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

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
	using namespace labrador;

	Glyph cell(char32_t character)
	{
		Glyph glyph;
		glyph.character = character;
		glyph.subrect = RectangleI(0, 0, 10, 10);
		return glyph;
	}

	// A renderer with no graphics API, an atlas built without a file and a
	// two-glyph font over it - ten by ten, stepping ten, on a line twenty
	// tall, so "AB" is two sprites and measures twenty by twenty.
	class Harness
	{
	public:
		Harness()
		{
			this->renderer_.create_device(nullptr, 640, 480, 1);
			this->renderer_.set_resources(&this->resources);

			TextureData texture;
			texture.width = 16;
			texture.height = 16;
			texture.format = TextureFormat::r8g8b8a8_unorm;
			texture.levels.push_back(texture_level(texture.format, 16, 16, 0));
			texture.pixels.assign(texture.levels[0].size, 0xFFu);
			add_texture_asset(this->renderer_, this->resources, "atlas",
				texture);

			std::vector<Glyph> glyphs;
			glyphs.push_back(cell(U'A'));
			glyphs.push_back(cell(U'B'));
			this->resources.add_font("font",
				std::make_unique<Font>(this->resources.resolve_texture("atlas"),
					std::move(glyphs), 20.0f));
		}

		// How many sprites one draw of `object` records. A copy of the count,
		// because the recording is valid only until the next begin_frame.
		int sprites_drawn(const UiObject& object)
		{
			this->renderer_.begin_frame();
			this->renderer_.set_view_count(1);
			DrawList list = this->renderer_.view(0);
			object.draw(list);
			this->renderer_.submit();
			return static_cast<int>(recorded_sprites(this->renderer_).size());
		}

		// The table before the renderer, so it dies after one -
		// render_resources.h states that ordering as a term of the seam.
		RenderResources resources;

	private:
		Renderer renderer_;
	};
}

TEST_CASE("a hidden UiText draws nothing and keeps its words, box and colour")
{
	Harness harness;
	const Colour colour(0.25f, 0.5f, 0.75f, 1.0f);
	UiText text("label", L"AB", "font", Vector2F(100.0f, 50.0f),
		&harness.resources, colour);

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
		&harness.resources, Colour::white, true);

	CHECK(harness.sprites_drawn(text) == 0);
	CHECK(text.bounds() == RectangleF(0.0f, 0.0f, 20.0f, 20.0f));
}
