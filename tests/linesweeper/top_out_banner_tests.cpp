#include <doctest/doctest.h>

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/camera.h"
#include "engine/render/font.h"
#include "engine/render/null/recording.h"
#include "engine/render/render_resources.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/render/viewport.h"
#include "engine/scene/scene.h"
#include "samples/linesweeper/presentation/layout.h"
#include "samples/linesweeper/presentation/top_out_banner.h"
#include "samples/linesweeper/rules/world.h"

#include <array>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

// The null renderer records actual sprite geometry without a window or GPU.
// Partial views exercise Scene's cull, rather than drawing the banner directly
// and assuming Scene would have admitted it. Captures own visual qualification.
namespace
{
	using namespace labrador;
	using namespace linesweeper;

	class Harness
	{
	public:
		Harness()
		{
			this->renderer_.create_device(nullptr, 1280, 720, 1);
			this->renderer_.set_resources(&this->resources_);

			TextureData texture;
			texture.width = texture.height = 1;
			texture.levels.push_back(texture_level(texture.format, 1, 1, 0));
			texture.pixels.assign(4, 255);
			add_texture_asset(this->renderer_, this->resources_, "white", texture);
			this->white = this->resources_.resolve_texture("white");
			load_font_asset(this->renderer_, this->resources_,
				LINESWEEPER_FONT_DIRECTORY, "jetbrains_mono_bold_16");
		}

		std::vector<RecordedSprite> draw(const TopOutBanner& banner,
			const Camera& camera = Camera::default_camera)
		{
			this->renderer_.begin_frame();
			this->renderer_.set_view_count(1);
			DrawList list = this->renderer_.view(0);
			list.set_camera(camera);
			list.set_viewport(Viewport(0.0f, 0.0f, 1280.0f, 720.0f));
			banner.draw(list);
			this->renderer_.submit();
			return recorded_sprites(this->renderer_);
		}

		std::vector<RecordedSprite> draw(const Scene& scene)
		{
			this->renderer_.begin_frame();
			scene.draw(this->renderer_);
			this->renderer_.submit();
			return recorded_sprites(this->renderer_);
		}

		RenderResources& resources() { return this->resources_; }

		TextureHandle white;

	private:
		RenderResources resources_;
		Renderer renderer_;
	};

	RectangleF sprite_bounds(const RecordedSprite& sprite,
		const Camera& camera = Camera::default_camera)
	{
		std::array<Point2F, 4> points;
		for (std::size_t index = 0; index < points.size(); ++index)
		{
			points[index] = sprite.corners[index].position / camera.scale +
				camera.translation;
		}
		return RectangleF::bounding_box_of(points);
	}

	World finished_match()
	{
		World world;
		world.topped_out = 1;
		return world;
	}
}

TEST_CASE("top-out banner bounds enclose both restart messages beyond the panel")
{
	Harness harness;
	const World world = finished_match();
	TopOutBanner banner(&world, &harness.resources());
	for (const bool touch : { false, true })
	{
		CAPTURE(touch);
		banner.set_touch(touch);
		banner.update(0.0f);
		const RectangleF bounds = banner.bounds();
		const std::vector<RecordedSprite> sprites = harness.draw(banner);
		REQUIRE(sprites.size() > 1);
		CHECK(bounds.left() < well_origin_x);
		CHECK(bounds.right() > well_origin_x + well_width);
		for (const RecordedSprite& sprite : sprites)
		{
			CHECK(bounds.contains(sprite_bounds(sprite)));
		}
		CHECK(bounds.left() == (touch ? 463.0f : 450.0f));
		CHECK(bounds.right() == (touch ? 816.0f : 829.0f));
	}
}

TEST_CASE("Scene retains top-out banner glyphs in views that do not intersect its panel")
{
	Harness harness;
	const World world = finished_match();
	Scene scene(nullptr, nullptr);
	TopOutBanner* banner = scene.add(std::make_unique<TopOutBanner>(
		&world, &harness.resources()));
	scene.end_tick();

	for (const bool touch : { false, true })
	{
		banner->set_touch(touch);
		scene.update(0.0f);
		const std::array<RectangleF, 2> visible = {
			RectangleF(touch ? 460.0f : 445.0f, 340.0f, 35.0f, 70.0f),
			RectangleF(790.0f, 340.0f, touch ? 30.0f : 45.0f, 70.0f),
		};
		for (const RectangleF& region : visible)
		{
			for (const float scale : { 0.5f, 1.0f, 2.0f })
			{
				CAPTURE(touch);
				CAPTURE(region.x);
				CAPTURE(scale);
				const Viewport viewport(0.0f, 0.0f,
					region.width * scale, region.height * scale);
				scene.clear_views();
				scene.add_view(viewport, Camera(region.position(), scale));
				const std::vector<RecordedSprite> sprites = harness.draw(scene);
				REQUIRE(sprites.size() > 1);
				bool visible_glyph = false;
				for (const RecordedSprite& sprite : sprites)
				{
					if (sprite.texture != harness.white &&
						sprite_bounds(sprite).intersects(viewport.rectangle()))
					{
						visible_glyph = true;
					}
				}
				CHECK(visible_glyph);
			}
		}
	}
}

TEST_CASE("top-out banner culling encloses view-quantized panel and unsnapped glyphs")
{
	Harness harness;
	const World world = finished_match();
	const TopOutBanner banner(&world, &harness.resources());
	const std::array<Camera, 3> cameras = {
		Camera::default_camera,
		Camera(Vector2F(450.5f, 345.75f), 0.25f),
		Camera(Vector2F(830.5f, 360.25f), 2.0f),
	};
	for (const Camera& camera : cameras)
	{
		CAPTURE(camera.scale);
		const RectangleF cull = banner.cull_bounds(1.0f / camera.scale);
		const std::vector<RecordedSprite> sprites = harness.draw(banner, camera);
		REQUIRE(sprites.size() > 1);
		for (const RecordedSprite& sprite : sprites)
		{
			CHECK(cull.contains(sprite_bounds(sprite, camera)));
		}
	}
}

TEST_CASE("top-out banner bounds include glyph bearings above the measured layout")
{
	Harness harness;
	const TextureHandle atlas = harness.resources().font(
		harness.resources().resolve_sprite_font("jetbrains_mono_bold_16"))->atlas();
	Glyph glyph;
	glyph.character = U'T';
	glyph.subrect = RectangleI(0, 0, 10, 12);
	glyph.y_offset = -40.0f;
	glyph.x_advance = 1.0f;
	std::vector<Glyph> glyphs = { glyph };
	std::unique_ptr<Font> font =
		std::make_unique<Font>(atlas, std::move(glyphs), 16.0f);
	font->set_stand_in(U'T');
	harness.resources().add_font("jetbrains_mono_bold_16", std::move(font));

	const World world = finished_match();
	const TopOutBanner banner(&world, &harness.resources());
	const RectangleF bounds = banner.bounds();
	CHECK(bounds.top() == well_origin_y + well_height * 0.5f - 40.0f);
	const std::vector<RecordedSprite> sprites = harness.draw(banner);
	REQUIRE(sprites.size() > 1);
	for (const RecordedSprite& sprite : sprites)
	{
		CHECK(bounds.contains(sprite_bounds(sprite)));
	}
}

TEST_CASE("top-out banner cache refreshes on match and touch changes without changing full-view draws")
{
	Harness harness;
	World world;
	Scene scene(nullptr, nullptr);
	TopOutBanner* banner = scene.add(std::make_unique<TopOutBanner>(
		&world, &harness.resources()));
	scene.end_tick();
	scene.add_view(Viewport(0.0f, 0.0f, 1280.0f, 720.0f),
		Camera::default_camera);
	CHECK(harness.draw(scene).empty());

	world.topped_out = 1;
	scene.update(0.0f);
	const RectangleF keyboard_bounds = banner->bounds();
	banner->set_touch(true);
	CHECK(banner->bounds() == keyboard_bounds);

	for (const bool touch : { false, true })
	{
		CAPTURE(touch);
		banner->set_touch(touch);
		scene.update(0.0f);
		const std::vector<RecordedSprite> direct = harness.draw(*banner);
		const std::vector<RecordedSprite> culled = harness.draw(scene);
		REQUIRE(culled.size() == direct.size());
		REQUIRE(culled.size() == (touch ? 23u : 24u));
		CHECK(culled.front().texture == harness.white);
		const RectangleF panel = sprite_bounds(culled.front());
		CHECK(panel.left() == well_origin_x);
		CHECK(panel.right() == well_origin_x + well_width);
		CHECK(panel.top() == well_origin_y + well_height * 0.5f - 10.0f);
		for (std::size_t index = 0; index < direct.size(); ++index)
		{
			CHECK(culled[index].texture == direct[index].texture);
			CHECK(culled[index].filter == direct[index].filter);
			CHECK(culled[index].viewport == direct[index].viewport);
			for (int corner = 0; corner < 4; ++corner)
			{
				CHECK(culled[index].corners[corner].position ==
					direct[index].corners[corner].position);
				CHECK(culled[index].corners[corner].texcoord ==
					direct[index].corners[corner].texcoord);
				CHECK(culled[index].corners[corner].colour ==
					direct[index].corners[corner].colour);
			}
		}
	}

	world.topped_out = 0;
	scene.update(0.0f);
	CHECK(harness.draw(scene).empty());
}
