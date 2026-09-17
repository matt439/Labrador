#include <doctest/doctest.h>

#include "engine/render/null/recording.h"
#include "engine/core/name_table.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/animation_strip.h"
#include "engine/render/camera.h"
#include "engine/render/colour.h"
#include "engine/render/font.h"
#include "engine/render/label.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/sprite_frame.h"
#include "engine/render/sprite_sheet.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"
#include "engine/render/viewport.h"
#include "engine/render/visual.h"
#include "engine/scene/scene.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

// The cull, asked through its consumer.
//
// "Objects expose bounds; the scene culls" (PHILOSOPHY, Rendering) is two
// halves, and every test of the first half checked what bounds() returned
// against what the test expected bounds() to return. What a cull needs is for
// bounds() to enclose what draw() draws, and only Scene::draw can say whether
// it does: a sprite whose box and whose quad disagree is submitted or dropped
// by the scene, and nothing else in the tree can see which
// (docs/review/gpt6/README.md, G6-04). So these cases put an object at the
// edge of a view, draw the scene, and count what the recording holds.
//
// COMPILED ONLY IN THE null CONFIGURATION, for fanout_tests.cpp's reason:
// Scene::draw takes a Renderer, and only the null backend has one that needs
// no window and no adapter.

namespace
{
	using namespace labrador;
	using namespace mattmath;

	// A renderer with no graphics API, a two-texel texture, and a sheet with
	// one frame whose authored origin is its own far corner - the shape a
	// sheet author uses to pin a sprite by its bottom right, and the shape
	// the old bounds() did not account for.
	class Harness
	{
	public:
		Harness()
		{
			this->renderer_.create_device(nullptr, 1280, 720, 1);
			this->renderer_.set_resources(&this->resources_);

			TextureData texture;
			texture.width = 8;
			texture.height = 8;
			texture.format = TextureFormat::r8g8b8a8_unorm;
			texture.levels.push_back(texture_level(texture.format, 8, 8, 0));
			texture.pixels.assign(texture.levels[0].size, 0xFFu);
			add_texture_asset(this->renderer_, this->resources_, "quad",
				texture);

			NameTable<SpriteFrame> frames("sprite frame");
			frames.add("plain", SpriteFrame(RectangleI(0, 0, 8, 8)));
			frames.add("pinned", SpriteFrame(RectangleI(0, 0, 8, 8),
				Vector2F(8.0f, 8.0f)));
			this->resources_.add_sprite_sheet("sheet",
				std::make_unique<SpriteSheet>(
					this->resources_.resolve_texture("quad"),
					std::move(frames), NameTable<AnimationStrip>("strip")));

			// Ten by ten cells on a twenty-tall line, as label_tests.cpp
			// builds one, so "AB" measures exactly twenty by twenty.
			std::vector<Glyph> glyphs;
			Glyph a;
			a.character = U'A';
			a.subrect = RectangleI(0, 0, 10, 10);
			glyphs.push_back(a);
			Glyph b = a;
			b.character = U'B';
			glyphs.push_back(b);
			this->resources_.add_font("font",
				std::make_unique<Font>(this->resources_.resolve_texture("quad"),
					std::move(glyphs), 20.0f));
		}

		// One view, ninety pixels square at the world's origin: an object is
		// inside it when any of its drawn pixels has x < 90.
		std::unique_ptr<Scene> scene_seeing_ninety()
		{
			std::unique_ptr<Scene> scene =
				std::make_unique<Scene>(nullptr, nullptr);
			const Viewport viewport(0.0f, 0.0f, 90.0f, 90.0f);
			scene->add_view(viewport,
				Camera::frame(RectangleF(0.0f, 0.0f, 90.0f, 90.0f), viewport));
			return scene;
		}

		size_t sprites_drawn(const Scene& scene)
		{
			this->renderer_.begin_frame();
			scene.draw(this->renderer_);
			this->renderer_.submit();
			return recorded_sprites(this->renderer_).size();
		}

		RenderResources* resources() { return &this->resources_; }

	private:
		RenderResources resources_;
		Renderer renderer_;
	};
}

TEST_CASE("CONTRACT: a sprite whose origin puts it in view is drawn")
{
	// The review's first row. The rectangle is x=100..120, wholly outside a
	// view that ends at x=90; the frame's origin draws the sprite across
	// x=80..100, ten pixels of it inside. Culled against the rectangle it
	// was never submitted, and that happened on every backend at once.
	Harness harness;
	std::unique_ptr<Scene> scene = harness.scene_seeing_ninety();
	scene->add(std::make_unique<Visual>("sheet", "pinned",
		RectangleF(100.0f, 20.0f, 20.0f, 20.0f), harness.resources()));
	scene->end_tick();

	CHECK(harness.sprites_drawn(*scene) == 1);
}

TEST_CASE("CONTRACT: a sprite turned into view is drawn")
{
	// The second row: no origin, a half turn about the top left.
	Harness harness;
	const float HALF_TURN = 3.14159265f;
	std::unique_ptr<Scene> scene = harness.scene_seeing_ninety();
	scene->add(std::make_unique<Visual>("sheet", "plain",
		RectangleF(100.0f, 20.0f, 20.0f, 20.0f), harness.resources(),
		Colour::white, HALF_TURN));
	scene->end_tick();

	CHECK(harness.sprites_drawn(*scene) == 1);
}

TEST_CASE("CONTRACT: a label turned into view is drawn")
{
	// The third row: two glyphs, so two sprites, both of them on the far
	// side of the position once turned.
	Harness harness;
	const float HALF_TURN = 3.14159265f;
	std::unique_ptr<Scene> scene = harness.scene_seeing_ninety();
	scene->add(std::make_unique<Label>(L"AB", "font",
		Vector2F(100.0f, 20.0f), harness.resources(), Colour::white, 1.0f,
		HALF_TURN));
	scene->end_tick();

	CHECK(harness.sprites_drawn(*scene) == 2);
}

TEST_CASE("CONTRACT: a sprite whose drawn pixels are all outside is not")
{
	// The cull still culls. Same rectangle, no origin and no turn: every
	// pixel is at x >= 100, and the scene submits nothing for it. Without
	// this case the three above would pass against a scene that stopped
	// culling altogether.
	Harness harness;
	std::unique_ptr<Scene> scene = harness.scene_seeing_ninety();
	scene->add(std::make_unique<Visual>("sheet", "plain",
		RectangleF(100.0f, 20.0f, 20.0f, 20.0f), harness.resources()));
	scene->end_tick();

	CHECK(harness.sprites_drawn(*scene) == 0);
}
