#include <doctest/doctest.h>

#include "tests/allocation_scope.h"

// Holds the contact sweep to PHILOSOPHY's Performance rule that per-frame code
// performs no heap allocation: contacts.h and broad_phase.h promise that a
// caller keeping its contact vector and its BroadPhase across frames
// allocates nothing once they are warm.
//
// The world here repeats every second tick, so after a few warming frames
// every measured frame is one whose pair and contact counts have already been
// seen. Growth beyond the warmed peak would be a new workload, not a frame.
//
// Debug builds against the DLL CRT only, where allocation_scope.h has its
// hook; everywhere else this file compiles to nothing.
#if defined(_MSC_VER) && defined(_DEBUG) && defined(_DLL)

#include "engine/collision/broad_phase.h"
#include "engine/collision/collision_layer.h"
#include "engine/collision/collision_object.h"
#include "engine/collision/contacts.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "tests/collision/stub_collider.h"

#include <cstddef>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace
{
	using labrador::BroadPhase;
	using labrador::CollisionLayer;
	using labrador::CollisionObject;
	using labrador::Contact;
	using labrador::RectangleF;
	using labrador::Vector2F;

	constexpr CollisionLayer body_layer = 1u << 0;

	// Steps `step` one tick and back the next, so every second tick repeats
	// a position, and counts the contacts it is told about.
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

	// A 16 by 16 lattice of boxes at a pitch smaller than their side, so
	// neighbours overlap, with every other one moving; and one floor wide
	// enough that the grid keeps it in its large-object list.
	struct Field
	{
		std::vector<std::unique_ptr<Oscillator>> owned;
		std::vector<CollisionObject*> objects;

		Field()
		{
			constexpr int side = 16;
			for (int y = 0; y < side; ++y)
			{
				for (int x = 0; x < side; ++x)
				{
					const bool moves = (x + y) % 2 == 0;
					this->owned.push_back(std::make_unique<Oscillator>(
						RectangleF(static_cast<float>(x) * 9.0f,
							static_cast<float>(y) * 9.0f, 10.0f, 10.0f),
						moves ? Vector2F(3.0f, 1.0f) : Vector2F::zero));
				}
			}
			this->owned.push_back(std::make_unique<Oscillator>(
				RectangleF(-1000.0f, 140.0f, 4000.0f, 20.0f), Vector2F::zero));
			for (const std::unique_ptr<Oscillator>& object : this->owned)
			{
				this->objects.push_back(object.get());
			}
		}

		void step()
		{
			for (const std::unique_ptr<Oscillator>& object : this->owned)
			{
				object->update(1.0f / 60.0f);
			}
		}
	};
}

TEST_CASE("a warmed contact sweep and dispatch allocate nothing, with and without the grid")
{
	labrador_tests::require_allocation_scope_counts();

	for (const bool grid : { true, false })
	{
		CAPTURE(grid);
		Field field;
		std::vector<Contact> contacts;
		BroadPhase broad_phase;
		BroadPhase* const chosen = grid ? &broad_phase : nullptr;
		const std::span<CollisionObject* const> objects(field.objects);

		const auto frame = [&]()
		{
			field.step();
			labrador::find_contacts(objects, contacts, chosen);
			labrador::dispatch_contacts(contacts);
		};

		for (int warm = 0; warm < 4; ++warm)
		{
			frame();
		}
		// A sweep that found nothing would allocate nothing for the wrong
		// reason.
		REQUIRE(contacts.size() > 100u);
		if (grid)
		{
			REQUIRE(broad_phase.large_object_count() == 1);
		}

		for (int measured = 0; measured < 8; ++measured)
		{
			CAPTURE(measured);
			const labrador_tests::AllocationSample sample =
				labrador_tests::count_allocations(frame);
			INFO("normal-block attempts ", sample.attempts, ", requested bytes ",
				sample.bytes, ", contacts ", contacts.size());
			CHECK(sample.attempts == 0);
		}
		CHECK(field.owned.front()->contacts() > 0);
	}
}

#endif
