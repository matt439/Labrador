#pragma once

#include "engine/math/rectanglei.h"
#include "engine/render/font.h"
#include "engine/render/null/recording.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

// A null renderer and its table, filled from bytes rather than files.
//
// null_test_harness.h loads ./content through the real loaders, which is what
// a test of the load path wants. A test of what a scene or a widget submits
// wants nothing beside the executable that could fail it for another reason,
// so this builds its textures and its font in memory instead. Each test file
// composes one and keeps only its own content and draw helpers.
//
// COMPILED ONLY IN THE null CONFIGURATION, by the files that include it: only
// the null backend has a Renderer that needs no window and no adapter.
class NullSyntheticHarness
{
public:
	NullSyntheticHarness(int width, int height, int view_capacity)
	{
		// No window: the null backend takes one and ignores it. The table is
		// attached before any content is added, as the seam requires.
		this->renderer_.create_device(nullptr, width, height, view_capacity);
		this->renderer_.set_resources(&this->resources_);
	}

	// A square texture of `size` texels, every byte 0xFF, under `name`.
	labrador::TextureHandle add_white_square(const std::string& name, int size)
	{
		labrador::TextureData texture;
		texture.width = size;
		texture.height = size;
		texture.format = labrador::TextureFormat::r8g8b8a8_unorm;
		texture.levels.push_back(
			labrador::texture_level(texture.format, size, size, 0));
		texture.pixels.assign(texture.levels[0].size, 0xFFu);
		labrador::add_texture_asset(this->renderer_, this->resources_, name,
			texture);
		return this->resources_.resolve_texture(name);
	}

	// A font named `name` over the texture `atlas`, with two glyphs, A and B:
	// ten by ten, stepping ten, on a line twenty tall, so "AB" is two sprites
	// and measures twenty by twenty.
	void add_two_glyph_font(const std::string& name, const std::string& atlas)
	{
		std::vector<labrador::Glyph> glyphs;
		glyphs.push_back(cell(U'A'));
		glyphs.push_back(cell(U'B'));
		this->resources_.add_font(name, std::make_unique<labrador::Font>(
			this->resources_.resolve_texture(atlas), std::move(glyphs), 20.0f));
	}

	labrador::Renderer& renderer() { return this->renderer_; }
	const labrador::Renderer& renderer() const { return this->renderer_; }
	labrador::RenderResources& resources() { return this->resources_; }

	// Valid until the next begin_frame, as recording.h says. A test comparing
	// two frames copies it.
	const std::vector<labrador::RecordedSprite>& recording() const
	{
		return labrador::recorded_sprites(this->renderer_);
	}

private:
	static labrador::Glyph cell(char32_t character)
	{
		labrador::Glyph glyph;
		glyph.character = character;
		glyph.subrect = labrador::RectangleI(0, 0, 10, 10);
		return glyph;
	}

	// The table before the renderer, so it dies after one -
	// render_resources.h states that ordering as a term of the seam.
	labrador::RenderResources resources_;
	labrador::Renderer renderer_;
};
