#include <doctest/doctest.h>

#include "tests/allocation_scope.h"

// Holds Scene's tick to PHILOSOPHY's Performance rule that per-frame code
// performs no heap allocation: scene.h keeps the contact list, the broad
// phase and the sweep's pointer list across ticks so that a busy tick
// allocates nothing after the first. Spawning and removing objects are the
// game's choices, not the frame's, so this world only moves.
//
// The world repeats every second tick, so after a few warming ticks every
// measured tick is one whose contacts the buffers have already held.
//
// Debug builds against the DLL CRT only, where allocation_scope.h has its
// hook; everywhere else this file compiles to nothing.
#if defined(_MSC_VER) && defined(_DEBUG) && defined(_DLL)

#include "engine/collision/collision_layer.h"
#include "engine/collision/collision_object.h"
#include "engine/core/game_object.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/scene/scene.h"
#include "tests/collision/stub_collider.h"

#include <cstddef>
#include <memory>

namespace
{
	using labrador::CollisionLayer;
	using labrador::CollisionObject;
	using labrador::GameObject;
	using labrador::RectangleF;
	using labrador::Scene;
	using labrador::Vector2F;

	constexpr CollisionLayer body_layer = 1u << 0;

	// Steps `step` one tick and back the next, and counts its contacts.
	class Oscillator final : public StubCollider
	{
	public:
		Oscillator(const RectangleF& rectangle, const Vector2F& step) :
			StubCollider(rectangle, body_layer, body_layer), step_(step)
		{
		}

		void update(float /*dt*/) override
		{
			this->rectangle_.offset(this->forward_ ? this->step_ : -this->step_);
			this->forward_ = !this->forward_;
		}

		void on_contact(const CollisionObject& /*other*/,
			const Vector2F& /*normal*/, float /*penetration*/) override
		{
			++this->contacts_;
		}

		int contacts() const { return this->contacts_; }

	private:
		Vector2F step_;
		bool forward_ = true;
		int contacts_ = 0;
	};

	// A plain object, which the tick steps after the collision objects.
	class Counter final : public GameObject
	{
	public:
		void update(float /*dt*/) override { ++this->updates_; }
		void draw(labrador::DrawList& /*draw_list*/) const override {}
		RectangleF bounds() const override { return RectangleF(0, 0, 1, 1); }

		int updates() const { return this->updates_; }

	private:
		int updates_ = 0;
	};
}

TEST_CASE("a warmed Scene tick of update, resolve and end_tick allocates nothing")
{
	labrador_tests::require_allocation_scope_counts();

	Scene scene(nullptr, nullptr);
	scene.set_bounds(RectangleF(-2000.0f, -2000.0f, 6000.0f, 6000.0f));

	constexpr int side = 16;
	Oscillator* first = nullptr;
	for (int y = 0; y < side; ++y)
	{
		for (int x = 0; x < side; ++x)
		{
			const bool moves = (x + y) % 2 == 0;
			Oscillator* added = scene.add(std::make_unique<Oscillator>(
				RectangleF(static_cast<float>(x) * 9.0f,
					static_cast<float>(y) * 9.0f, 10.0f, 10.0f),
				moves ? Vector2F(3.0f, 1.0f) : Vector2F::zero));
			if (first == nullptr)
			{
				first = added;
			}
		}
	}
	// One floor the grid keeps in its large-object list.
	scene.add(std::make_unique<Oscillator>(
		RectangleF(-1000.0f, 140.0f, 4000.0f, 20.0f), Vector2F::zero));
	Counter* counter = nullptr;
	for (int index = 0; index < 32; ++index)
	{
		counter = scene.add(std::make_unique<Counter>());
	}
	scene.end_tick();

	size_t contacts = 0;
	const auto tick = [&]()
	{
		scene.update(1.0f / 60.0f);
		scene.resolve();
		contacts = scene.contacts().size();
		scene.end_tick();
	};

	for (int warm = 0; warm < 4; ++warm)
	{
		tick();
	}
	// A tick that found nothing would allocate nothing for the wrong reason.
	REQUIRE(contacts > 100u);

	for (int measured = 0; measured < 8; ++measured)
	{
		CAPTURE(measured);
		const labrador_tests::AllocationSample sample =
			labrador_tests::count_allocations(tick);
		INFO("normal-block attempts ", sample.attempts, ", requested bytes ",
			sample.bytes, ", contacts ", contacts);
		CHECK(sample.attempts == 0);
	}
	CHECK(first->contacts() > 0);
	CHECK(counter->updates() == 12);
	CHECK(scene.collision_objects().size() == side * side + 1u);
}

#endif
