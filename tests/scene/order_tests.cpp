#include <doctest/doctest.h>

#include "engine/render/null/recording.h"
#include "engine/collision/collision_object.h"
#include "engine/core/state.h"
#include "engine/core/state_context.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/viewport.h"
#include "engine/scene/scene.h"
#include "tests/collision/stub_collider.h"
#include "tests/render/null_synthetic_harness.h"

#include <memory>
#include <utility>
#include <vector>

// What draws over what, which is the order a scene submits in (scene.h,
// draw()). The seam draws a list in call order and reads no depth
// (renderer.h, draw_sprite), so this order is the only depth a scene has, and
// both clients build their layers out of it.
//
// COMPILED ONLY IN THE null CONFIGURATION, for fanout_tests.cpp's reason:
// Scene::draw takes a Renderer, and only the null backend has one that needs
// no window and no adapter.

namespace
{
	using namespace labrador;

	// Every object draws one 1x1 sprite at x = its id, so the recording's x
	// coordinates spell the order it was drawn in.
	void draw_marker(DrawList& list, TextureHandle white, int id)
	{
		list.draw_sprite(white, RectangleI(0, 0, 1, 1),
			RectangleF(static_cast<float>(id), 0.0f, 1.0f, 1.0f),
			Colour::white, 0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
	}

	class Plain final : public GameObject
	{
	public:
		Plain(int id, TextureHandle white) : id_(id), white_(white) {}

		void update(float /*dt*/) override {}
		void draw(DrawList& list) const override
		{
			draw_marker(list, this->white_, this->id_);
		}
		RectangleF bounds() const override
		{
			return RectangleF(static_cast<float>(this->id_), 0.0f, 1.0f, 1.0f);
		}

	private:
		int id_ = 0;
		TextureHandle white_;
	};

	// The shared stub on no layer, drawing its marker.
	class Collider final : public StubCollider
	{
	public:
		Collider(int id, TextureHandle white) :
			StubCollider(RectangleF(static_cast<float>(id), 0.0f, 1.0f, 1.0f),
				0, 0),
			id_(id), white_(white)
		{
		}

		void draw(DrawList& list) const override
		{
			draw_marker(list, this->white_, this->id_);
		}

	private:
		int id_ = 0;
		TextureHandle white_;
	};

	class Harness
	{
	public:
		Harness() :
			null_(64, 64, 1)
		{
			add_white_texture(this->null_.renderer(), this->null_.resources());
			this->white =
				this->null_.resources().resolve_texture(white_texture_name);
		}

		Renderer& renderer() { return this->null_.renderer(); }

		// The ids in the order the frame drew them.
		std::vector<int> drawn() const
		{
			std::vector<int> ids;
			for (const RecordedSprite& sprite : this->null_.recording())
			{
				ids.push_back(static_cast<int>(sprite.corners[0].position.x));
			}
			return ids;
		}

		TextureHandle white;

	private:
		NullSyntheticHarness null_;
	};
}

TEST_CASE("CONTRACT: plain objects draw in insertion order, then collision objects")
{
	Harness harness;
	Scene scene(nullptr, nullptr);
	scene.add_view(Viewport(0, 0, 64, 64));

	// Interleaved on purpose: the split is by kind, not by when.
	scene.add(std::make_unique<Plain>(1, harness.white));
	scene.add(std::make_unique<Collider>(20, harness.white));
	scene.add(std::make_unique<Plain>(2, harness.white));
	scene.add(std::make_unique<Plain>(3, harness.white));
	scene.end_tick();

	harness.renderer().begin_frame();
	scene.draw(harness.renderer());
	harness.renderer().submit();

	CHECK(harness.drawn() == std::vector<int>{ 1, 2, 3, 20 });
}

TEST_CASE("CONTRACT: the overlay draws over the world, and a later scene over both")
{
	Harness harness;
	Scene world(nullptr, nullptr);
	world.add_view(Viewport(0, 0, 64, 64));
	world.add(std::make_unique<Collider>(10, harness.white));
	world.add(std::make_unique<Plain>(1, harness.white));
	world.end_tick();

	// A foreground layer is a second scene drawn after the first: a plain
	// object here covers every collision object in the world.
	Scene foreground(nullptr, nullptr);
	foreground.add_view(Viewport(0, 0, 64, 64));
	foreground.add(std::make_unique<Plain>(30, harness.white));
	foreground.end_tick();

	const TextureHandle white = harness.white;
	harness.renderer().begin_frame();
	world.draw(harness.renderer(), [white](int /*view_index*/, DrawList& list)
		{
			draw_marker(list, white, 20);
		});
	foreground.draw(harness.renderer());
	harness.renderer().submit();

	CHECK(harness.drawn() == std::vector<int>{ 1, 10, 20, 30 });
}

TEST_CASE("a removal keeps the plain objects' order")
{
	Harness harness;
	Scene scene(nullptr, nullptr);
	scene.add_view(Viewport(0, 0, 64, 64));
	scene.add(std::make_unique<Plain>(1, harness.white));
	Plain* middle = scene.add(std::make_unique<Plain>(2, harness.white));
	scene.add(std::make_unique<Plain>(3, harness.white));
	scene.end_tick();

	scene.remove(middle);
	scene.end_tick();

	harness.renderer().begin_frame();
	scene.draw(harness.renderer());
	harness.renderer().submit();

	CHECK(harness.drawn() == std::vector<int>{ 1, 3 });
}

TEST_CASE("CONTRACT: a retirement keeps the collision objects' order")
{
	Harness harness;
	Scene scene(nullptr, nullptr);
	scene.add_view(Viewport(0, 0, 64, 64));
	scene.add(std::make_unique<Collider>(10, harness.white));
	Collider* retiring = scene.add(std::make_unique<Collider>(11, harness.white));
	scene.add(std::make_unique<Collider>(12, harness.white));
	scene.add(std::make_unique<Collider>(13, harness.white));
	scene.end_tick();

	// Swap-and-pop would move 13 into 11's place and draw it under 12.
	retiring->set_for_deletion(true);
	scene.end_tick();

	harness.renderer().begin_frame();
	scene.draw(harness.renderer());
	harness.renderer().submit();

	CHECK(harness.drawn() == std::vector<int>{ 10, 12, 13 });
}

TEST_CASE("a state stack composes split gameplay and a fullscreen scene in order")
{
	// A state that covers only part of the screen leaves the one under it
	// drawn, so the stack is a layer order too: the lower state's scene draws
	// all its views, then the upper state's scene draws over them. Here the
	// lower scene is split into two side-by-side views and the upper one is a
	// single fullscreen view, and each frame records them in that order.
	class SceneState final : public State
	{
	public:
		SceneState(std::unique_ptr<Scene> scene, bool covers_screen) :
			scene_(std::move(scene)), covers_screen_(covers_screen)
		{
		}

		void init() override {}
		void update(float /*dt*/) override {}
		bool covers_screen() const override { return this->covers_screen_; }

		void draw(Renderer& renderer) const override
		{
			this->scene_->draw(renderer);
		}

	private:
		std::unique_ptr<Scene> scene_;
		bool covers_screen_ = true;
	};

	// One 2x2 texture, and a block that draws it across its own rectangle.
	class Block final : public GameObject
	{
	public:
		Block(const RectangleF& rectangle, TextureHandle texture) :
			rectangle_(rectangle), texture_(texture)
		{
		}

		void update(float /*dt*/) override {}

		void draw(DrawList& list) const override
		{
			list.draw_sprite(this->texture_, RectangleI(0, 0, 2, 2),
				this->rectangle_, Colour::white, 0.0f, Vector2F::zero,
				SpriteFlip::none, 0.0f);
		}

		RectangleF bounds() const override { return this->rectangle_; }

	private:
		RectangleF rectangle_;
		TextureHandle texture_;
	};

	NullSyntheticHarness null(1280, 720, 2);
	const TextureHandle quad = null.add_white_square("quad", 2);

	std::unique_ptr<Scene> split = std::make_unique<Scene>(nullptr, nullptr);
	split->add(std::make_unique<Block>(
		RectangleF(0.0f, 0.0f, 10.0f, 10.0f), quad));
	split->end_tick();
	split->add_view(Viewport(0.0f, 0.0f, 640.0f, 720.0f));
	split->add_view(Viewport(640.0f, 0.0f, 640.0f, 720.0f));

	std::unique_ptr<Scene> fullscreen = std::make_unique<Scene>(nullptr, nullptr);
	fullscreen->add(std::make_unique<Block>(
		RectangleF(20.0f, 20.0f, 10.0f, 10.0f), quad));
	fullscreen->end_tick();
	fullscreen->add_view(Viewport(0.0f, 0.0f, 1280.0f, 720.0f));

	StateContext states;
	states.transition_to(std::make_unique<SceneState>(std::move(split), true));
	states.push(std::make_unique<SceneState>(std::move(fullscreen), false));

	for (int frame = 0; frame < 2; ++frame)
	{
		null.renderer().begin_frame();
		states.draw(null.renderer());
		null.renderer().submit();

		const std::vector<RecordedSprite>& sprites = null.recording();
		REQUIRE(sprites.size() == 3);
		CHECK(sprites[0].viewport.width == 640);
		CHECK(sprites[1].viewport.x == 640);
		CHECK(sprites[2].viewport.width == 1280);
		CHECK(sprites[2].corners[0].position == Vector2F(20.0f, 20.0f));
		CHECK(null.renderer().view_count() == 2);
	}
}
