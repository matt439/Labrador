#include <doctest/doctest.h>

#include "samples/components/nine_slice.h"
#include "samples/components/slider.h"

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/font.h"
#include "engine/render/null/recording.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/ui/widget.h"

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using components::Bar;
using components::NineSlice;
using components::SliceBorder;
using components::SlicePiece;
using components::Slider;
using namespace labrador;

namespace
{
	// Compile this file only for native null. It uses no window or API
	// device: the recorder keeps every sprite a widget draws, with the
	// texture, rectangle and tint it was given.
	class DrawContent
	{
	public:
		DrawContent()
		{
			this->renderer.create_device(nullptr, 640, 360, 1);
			this->renderer.set_resources(&this->resources);
			this->panel = this->add_texture("panel", 16, 16);
			this->block = this->add_texture("block", 1, 1);

			// The slider's text needs a font with an atlas the recorder can
			// read, and one apart from the bars, so its glyphs can be told
			// from them.
			const TextureHandle glyph_atlas = this->add_texture("glyphs", 16, 16);
			std::vector<Glyph> glyphs;
			for (char32_t character = U' '; character <= U'~'; ++character)
			{
				Glyph glyph;
				glyph.character = character;
				glyph.subrect = RectangleI(0, 0, 10, 10);
				glyphs.push_back(glyph);
			}
			this->resources.add_font("font", std::make_unique<Font>(
				glyph_atlas, std::move(glyphs), 20.0f));
		}

		// One frame with only `widget` in it.
		std::vector<RecordedSprite> record(const UiWidget& widget)
		{
			this->renderer.begin_frame();
			this->renderer.set_view_count(1);
			DrawList list = this->renderer.view(0);
			widget.draw(list);
			this->renderer.submit();
			return recorded_sprites(this->renderer);
		}

		// Table before renderer: the renderer dies before the table it borrows.
		RenderResources resources;
		Renderer renderer;
		TextureHandle panel;
		TextureHandle block;

	private:
		TextureHandle add_texture(const std::string& name, int width,
			int height)
		{
			TextureData data;
			data.width = width;
			data.height = height;
			data.levels.push_back(texture_level(data.format, width, height, 0));
			data.pixels.assign(data.levels[0].size, 255);
			add_texture_asset(this->renderer, this->resources, name, data);
			return this->resources.resolve_texture(name);
		}
	};

	void check_drawn(const RecordedSprite& sprite, const RectangleF& destination)
	{
		CHECK(sprite.corners[0].position == Vector2F(destination.x, destination.y));
		CHECK(sprite.corners[3].position == Vector2F(
			destination.x + destination.width, destination.y + destination.height));
	}

	// A 16-texel frame with a 6-texel corner on the left and top and a
	// 4-texel one on the right and bottom, as nine_slice_tests.cpp uses.
	const RectangleI frame(0, 0, 16, 16);
	const SliceBorder border{ 6, 6, 4, 4 };
}

TEST_CASE("a nine-slice draws each of its pieces from its texture, in its colour")
{
	DrawContent content;
	const Colour tint(10, 20, 30);
	const NineSlice panel("panel", content.panel, frame, border,
		RectangleF(10.0f, 20.0f, 100.0f, 40.0f), tint);

	const std::vector<RecordedSprite> drawn = content.record(panel);
	REQUIRE(drawn.size() == 9);
	const std::array<SlicePiece, 9>& pieces = panel.pieces();
	for (size_t index = 0; index < drawn.size(); ++index)
	{
		const RecordedSprite& sprite = drawn[index];
		const SlicePiece& piece = pieces[index];
		CHECK(sprite.texture == content.panel);
		CHECK(sprite.corners[0].colour == tint);
		check_drawn(sprite, piece.destination);

		// Sixteen texels across, so each source edge is a sixteenth.
		CHECK(sprite.corners[0].texcoord == Vector2F(
			static_cast<float>(piece.source.x) / 16.0f,
			static_cast<float>(piece.source.y) / 16.0f));
		CHECK(sprite.corners[3].texcoord == Vector2F(
			static_cast<float>(piece.source.x + piece.source.width) / 16.0f,
			static_cast<float>(piece.source.y + piece.source.height) / 16.0f));
	}
}

TEST_CASE("a nine-slice skips the pieces with no texels or no area, and draws nothing hidden")
{
	DrawContent content;

	// A zero border leaves only the middle piece with texels.
	const NineSlice plain("plain", content.panel, frame, SliceBorder{},
		RectangleF(0.0f, 0.0f, 64.0f, 32.0f));
	const std::vector<RecordedSprite> stretched = content.record(plain);
	REQUIRE(stretched.size() == 1);
	check_drawn(stretched[0], RectangleF(0.0f, 0.0f, 64.0f, 32.0f));
	CHECK(stretched[0].corners[0].texcoord == Vector2F::zero);
	CHECK(stretched[0].corners[3].texcoord == Vector2F(1.0f, 1.0f));

	// Five units across ten texels of corner squeezes the middle column to
	// nothing, so its three pieces are not drawn and the other six are.
	const NineSlice squeezed("squeezed", content.panel, frame, border,
		RectangleF(0.0f, 0.0f, 5.0f, 50.0f));
	const std::vector<RecordedSprite> cornered = content.record(squeezed);
	REQUIRE(cornered.size() == 6);
	const std::array<SlicePiece, 9>& pieces = squeezed.pieces();
	const size_t kept[] = { 0, 2, 3, 5, 6, 8 };
	for (size_t index = 0; index < cornered.size(); ++index)
	{
		check_drawn(cornered[index], pieces[kept[index]].destination);
	}

	NineSlice hidden("hidden", content.panel, frame, border,
		RectangleF(10.0f, 20.0f, 100.0f, 40.0f));
	hidden.set_hidden(true);
	CHECK(content.record(hidden).empty());
}

TEST_CASE("a bar draws its rectangle from the block's texel, and nothing hidden")
{
	DrawContent content;
	const RectangleF rectangle(5.0f, 6.0f, 70.0f, 8.0f);
	Bar bar("bar", content.block, rectangle, Colour::red);

	const std::vector<RecordedSprite> drawn = content.record(bar);
	REQUIRE(drawn.size() == 1);
	CHECK(drawn[0].texture == content.block);
	CHECK(drawn[0].corners[0].colour == Colour::red);
	check_drawn(drawn[0], rectangle);

	bar.set_hidden(true);
	CHECK(content.record(bar).empty());
}

TEST_CASE("a slider draws its track, then its fill over it, and nothing hidden")
{
	DrawContent content;
	Slider slider("music", L"Music", 0, 100, 10, 50, Vector2F(100.0f, 50.0f),
		RectangleF(300.0f, 54.0f, 200.0f, 12.0f), "font", content.block,
		&content.resources);

	// The label and the number are glyphs from the font's atlas; the two
	// bars are the block's.
	std::vector<RecordedSprite> bars;
	for (const RecordedSprite& sprite : content.record(slider))
	{
		if (sprite.texture == content.block)
		{
			bars.push_back(sprite);
		}
	}
	REQUIRE(bars.size() == 2);
	check_drawn(bars[0], RectangleF(300.0f, 54.0f, 200.0f, 12.0f));
	CHECK(bars[0].corners[0].colour == slider.track().colour());
	check_drawn(bars[1], RectangleF(300.0f, 54.0f, 100.0f, 12.0f));
	CHECK(bars[1].corners[0].colour == slider.fill().colour());

	slider.set_hidden(true);
	CHECK(content.record(slider).empty());
}
