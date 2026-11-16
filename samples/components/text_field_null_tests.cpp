#include <doctest/doctest.h>

#include "samples/components/text_field.h"

#include "engine/math/rectanglei.h"
#include "engine/render/font.h"
#include "engine/render/null/recording.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"

#include <memory>
#include <utility>
#include <vector>

using components::TextField;
using namespace labrador;

TEST_CASE("TextField draws its cached end caret after spaces in repeated views without mutation")
{
	RenderResources resources;
	Renderer renderer;
	renderer.create_device(nullptr, 640, 360, 2);
	renderer.set_resources(&resources);
	TextureData texture;
	texture.width = 16;
	texture.height = 16;
	texture.levels.push_back(texture_level(texture.format, 16, 16, 0));
	texture.pixels.assign(texture.levels[0].size, 255);
	add_texture_asset(renderer, resources, "glyphs", texture);
	std::vector<Glyph> glyphs;
	for (char32_t character : { U'A', U'N', U'|', U' ' })
	{
		Glyph glyph;
		glyph.character = character;
		glyph.subrect = character == U' ' ? RectangleI(0, 0, 1, 1) :
			RectangleI(0, 0, 10, 10);
		glyph.x_advance = character == U' ' ? 9.0f : 0.0f;
		glyphs.push_back(glyph);
	}
	resources.add_font("font", std::make_unique<Font>(
		resources.resolve_texture("glyphs"), std::move(glyphs), 20.0f));
	TextField field("name", L"N", "A ", { 12, 30.0f },
		Vector2F::zero, 100.0f, "font", &resources);
	field.begin_edit();
	for (int frame = 0; frame < 2; ++frame)
	{
		renderer.begin_frame();
		renderer.set_view_count(2);
		for (int view = 0; view < 2; ++view)
		{
			DrawList list = renderer.view(view);
			field.draw(list);
		}
		renderer.submit();
		const std::vector<RecordedSprite>& drawn = recorded_sprites(renderer);
		REQUIRE(drawn.size() == 6);
		for (std::size_t view = 0; view < 2; ++view)
		{
			CHECK(drawn[view * 3].corners[0].position.x == 0.0f);
			CHECK(drawn[view * 3 + 1].corners[0].position.x == 100.0f);
			CHECK(drawn[view * 3 + 2].corners[0].position.x == 120.0f);
		}
		CHECK(field.value() == "A ");
		CHECK(field.draft() == "A ");
		CHECK(field.display().text() == L"A |");
	}
}
