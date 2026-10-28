#include <doctest/doctest.h>

#include "engine/assets/json.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/font.h"
#include "engine/render/render_resources.h"
#include "engine/render/text.h"
#include "engine/render/text_encoding.h"
#include "tests/scratch_file.h"

#if defined(LABRADOR_TEST_RENDER_NULL)
#include "engine/render/null/recording.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"
#endif

#include <array>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The text guide's game-side JSON recipe, compiled against the real engine.
// Ordinary cases use only font metadata. The null-only case records glyph
// submissions with a synthetic atlas; it makes no glyph-art or pixel claim.
namespace
{
	labrador::Text load_menu_title(labrador::RenderResources& resources,
		const char* path)
	{
		const labrador::JsonDocument document = labrador::read_json_file(path);
		const std::string utf8 = document.root().string("title");
		const std::wstring title = labrador::widen(utf8);
		const std::string font_name = "menu_bold_16";
		const labrador::FontHandle font = resources.resolve_sprite_font(font_name);
		if (!resources.can_render(font, title))
		{
			throw std::runtime_error(document.source_path() +
				": title has no glyph at UTF-16 unit " +
				std::to_string(resources.first_unrenderable(font, title)));
		}
		return labrador::Text(title, font_name,
			labrador::Vector2F(32.0f, 32.0f), &resources);
	}
	void add_menu_font(labrador::RenderResources& resources,
		bool include_accent, labrador::TextureHandle atlas = labrador::TextureHandle())
	{
		std::vector<labrador::Glyph> glyphs;
		for (char32_t character : { U'C', U'a', U'f', U'?' })
		{
			labrador::Glyph glyph;
			glyph.character = character;
			const int x = character == U'a' ? 4 : character == U'f' ? 8 : 0;
			glyph.subrect = labrador::RectangleI(x, 0, 4, 8);
			glyphs.push_back(glyph);
		}
		if (include_accent)
		{
			labrador::Glyph glyph;
			glyph.character = U'\u00E9';
			glyph.subrect = labrador::RectangleI(12, 0, 4, 8);
			glyphs.push_back(glyph);
		}
		std::unique_ptr<labrador::Font> font = std::make_unique<labrador::Font>(
			atlas, std::move(glyphs), 8.0f);
		font->set_stand_in(U'?');
		resources.add_font("menu_bold_16", std::move(font));
	}

	std::string rejection_from(labrador::RenderResources& resources,
		const labrador_tests::ScratchFile& file)
	{
		try
		{
			static_cast<void>(load_menu_title(resources, file.path().c_str()));
		}
		catch (const std::runtime_error& error)
		{
			return error.what();
		}
		FAIL("expected missing-glyph rejection");
		return {};
	}
}

TEST_SUITE("StringTableTextRecipeTests")
{
	TEST_CASE("UTF-8 and escaped JSON titles become owned wide Text")
	{
		labrador::RenderResources resources;
		add_menu_font(resources, true);
		const labrador::FontHandle font = resources.resolve_sprite_font("menu_bold_16");
		const std::array<std::string, 2> tables = {
			"{\"title\":\"Caf\xC3\xA9\"}",
			R"({"title":"Caf\u00E9"})"
		};
		for (std::size_t index = 0; index < tables.size(); ++index)
		{
			CAPTURE(index);
			labrador::Text title;
			{
				const labrador_tests::ScratchFile file("menu_title.json", tables[index]);
				title = load_menu_title(resources, file.path().c_str());
			}
			// Both the helper's document and the scratch file are gone.
			CHECK(title.text() == L"Caf\u00E9");
			CHECK(title.text().size() == 4);
			CHECK(resources.can_render(font, title.text()));
			CHECK(resources.first_unrenderable(font, title.text()) ==
				std::wstring_view::npos);
			CHECK(title.text_bounds() == labrador::RectangleF(32.0f, 32.0f, 16.0f, 8.0f));
		}
	}

	TEST_CASE("a missing title glyph reports its source and wide unit before drawing")
	{
		labrador::RenderResources resources;
		add_menu_font(resources, false);
		const labrador::FontHandle font = resources.resolve_sprite_font("menu_bold_16");
		const labrador_tests::ScratchFile file("missing_title.json",
			R"({"title":"Caf\u00E9"})");
		CHECK_FALSE(resources.can_render(font, L"Caf\u00E9"));
		CHECK(resources.first_unrenderable(font, L"Caf\u00E9") == 3);
		const std::string error = rejection_from(resources, file);
		CHECK(error.find(file.path()) != std::string::npos);
		CHECK(error.find(": title ") != std::string::npos);
		CHECK(error.find("UTF-16 unit 3") != std::string::npos);
	}

	TEST_CASE("a title outside the atlas is rejected despite a stand-in")
	{
		labrador::RenderResources resources;
		add_menu_font(resources, true);
		const labrador::FontHandle font = resources.resolve_sprite_font("menu_bold_16");
		CHECK(resources.font(font)->drawn(U'\u03A9').character == U'?');
		CHECK_FALSE(resources.can_render(font, L"\u03A9"));
		const labrador_tests::ScratchFile file("outside_title.json",
			R"({"title":"\u03A9"})");
		const std::string error = rejection_from(resources, file);
		CHECK(error.find(file.path()) != std::string::npos);
		CHECK(error.find(": title ") != std::string::npos);
		CHECK(error.find("UTF-16 unit 0") != std::string::npos);
	}

#if defined(LABRADOR_TEST_RENDER_NULL)
	TEST_CASE("the string-table Text records four glyphs with the null renderer")
	{
		labrador::RenderResources resources;
		labrador::Renderer renderer;
		renderer.create_device(nullptr, 64, 64, 1);
		renderer.set_resources(&resources);
		labrador::TextureData pixels;
		pixels.width = 16;
		pixels.height = 8;
		pixels.format = labrador::TextureFormat::r8g8b8a8_unorm;
		pixels.levels.push_back(labrador::texture_level(pixels.format, 16, 8, 0));
		pixels.pixels.assign(pixels.levels[0].size, 255);
		labrador::add_texture_asset(renderer, resources, "test:menu", pixels);
		const labrador::TextureHandle atlas = resources.resolve_texture("test:menu");
		add_menu_font(resources, true, atlas);
		labrador::Text title;
		{
			const labrador_tests::ScratchFile file("drawn_title.json",
				"{\"title\":\"Caf\xC3\xA9\"}");
			title = load_menu_title(resources, file.path().c_str());
		}
		renderer.begin_frame();
		renderer.set_view_count(1);
		labrador::DrawList draw_list = renderer.view(0);
		title.draw(draw_list);
		renderer.submit();
		const std::vector<labrador::RecordedSprite>& sprites =
			labrador::recorded_sprites(renderer);
		REQUIRE(sprites.size() == 4);
		for (const labrador::RecordedSprite& sprite : sprites)
		{
			CHECK(sprite.texture == atlas);
		}
		CHECK(sprites[0].corners[0].position == labrador::Vector2F(32.0f, 32.0f));
		CHECK(sprites[3].corners[0].position == labrador::Vector2F(44.0f, 32.0f));
		CHECK(sprites[3].corners[0].texcoord == labrador::Vector2F(0.75f, 0.0f));
		renderer.end_frame();
	}
#endif
}
