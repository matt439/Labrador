#include <doctest/doctest.h>

#include "engine/render/null/recording.h"
#include "engine/collision/collision_object.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/viewport.h"
#include "engine/scene/scene.h"

#include <memory>
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

	class Collider final : public CollisionObject
	{
	public:
		Collider(int id, TextureHandle white) :
			id_(id), white_(white),
			rectangle_(static_cast<float>(id), 0.0f, 1.0f, 1.0f)
		{
		}

		void update(float /*dt*/) override {}
		void draw(DrawList& list) const override
		{
			draw_marker(list, this->white_, this->id_);
		}
		RectangleF bounds() const override { return this->rectangle_; }

		const Shape* shape() const override { return &this->rectangle_; }
		CollisionLayer layer() const override { return 0; }
		CollisionMask mask() const override { return 0; }
		CollisionTag tag() const override { return 0; }
		void on_contact(const CollisionObject& /*other*/,
			const Vector2F& /*normal*/, float /*penetration*/) override
		{
		}
		bool for_deletion() const override { return this->for_deletion_; }
		void set_for_deletion(bool for_deletion) override
		{
			this->for_deletion_ = for_deletion;
		}

	private:
		int id_ = 0;
		TextureHandle white_;
		RectangleF rectangle_;
		bool for_deletion_ = false;
	};

	class Harness
	{
	public:
		Harness()
		{
			this->renderer_.create_device(nullptr, 64, 64, 1);
			this->renderer_.set_resources(&this->resources_);
			add_white_texture(this->renderer_, this->resources_);
			this->white = this->resources_.resolve_texture(white_texture_name);
		}

		Renderer& renderer() { return this->renderer_; }

		// The ids in the order the frame drew them.
		std::vector<int> drawn() const
		{
			std::vector<int> ids;
			for (const RecordedSprite& sprite : recorded_sprites(this->renderer_))
			{
				ids.push_back(static_cast<int>(sprite.corners[0].position.x));
			}
			return ids;
		}

		TextureHandle white;

	private:
		RenderResources resources_;
		Renderer renderer_;
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
