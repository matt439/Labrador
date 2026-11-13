#pragma once

#include "engine/core/game_object.h"
#include "engine/render/camera.h"
#include "engine/render/null/recording.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/render/viewport.h"
#include "engine/scene/scene.h"
#include "samples/linesweeper/presentation/assets.h"

#include <vector>

// The sample's drawables, drawn under the null renderer and read back as the
// sprites they submitted. Only the null-render configurations compile the
// files that include this (tests/linesweeper/CMakeLists.txt), which is where
// LINESWEEPER_FONT_DIRECTORY is defined.
//
// A null device at the sample's 1280x720, the white texel under the name the
// sample resolves, and the sample's own font read from the source tree through
// the same loader every backend uses. No window and no GPU: the null backend
// keeps a texture or a font atlas as a size.
namespace linesweeper_tests
{
	class NullHarness
	{
	public:
		NullHarness()
		{
			this->renderer_.create_device(nullptr, 1280, 720, 1);
			this->renderer_.set_resources(&this->resources_);

			labrador::TextureData texture;
			texture.width = texture.height = 1;
			texture.levels.push_back(
				labrador::texture_level(texture.format, 1, 1, 0));
			texture.pixels.assign(4, 255);
			labrador::add_texture_asset(this->renderer_, this->resources_,
				linesweeper::block_texture_name, texture);
			this->white =
				this->resources_.resolve_texture(linesweeper::block_texture_name);

			labrador::load_font_asset(this->renderer_, this->resources_,
				LINESWEEPER_FONT_DIRECTORY, linesweeper::font_name);
		}

		// One object drawn into one view through `camera`, over the whole
		// buffer. A copy, because the next begin_frame empties what
		// recorded_sprites refers to (tests/render/null_test_harness.h,
		// Harness::end).
		std::vector<labrador::RecordedSprite> draw(
			const labrador::GameObject& object,
			const labrador::Camera& camera = labrador::Camera::default_camera)
		{
			this->renderer_.begin_frame();
			this->renderer_.set_view_count(1);
			labrador::DrawList list = this->renderer_.view(0);
			list.set_camera(camera);
			list.set_viewport(labrador::Viewport(0.0f, 0.0f, 1280.0f, 720.0f));
			object.draw(list);
			this->renderer_.submit();
			return labrador::recorded_sprites(this->renderer_);
		}

		// A scene through its own views and its own cull.
		std::vector<labrador::RecordedSprite> draw(const labrador::Scene& scene)
		{
			this->renderer_.begin_frame();
			scene.draw(this->renderer_);
			this->renderer_.submit();
			return labrador::recorded_sprites(this->renderer_);
		}

		labrador::RenderResources& resources() { return this->resources_; }

		// For a test that drives frames itself and reads no copy back.
		labrador::Renderer& renderer() { return this->renderer_; }

		labrador::TextureHandle white;

	private:
		// The table before the renderer, so it dies after one
		// (render_resources.h).
		labrador::RenderResources resources_;
		labrador::Renderer renderer_;
	};
}
