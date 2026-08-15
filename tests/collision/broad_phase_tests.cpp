#include <doctest/doctest.h>

#include "engine/collision/broad_phase.h"
#include "engine/collision/collision_layer.h"
#include "engine/collision/collision_object.h"
#include "engine/collision/contacts.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using labrador::BroadPhase;
using labrador::CollisionLayer;
using labrador::CollisionMask;
using labrador::CollisionObject;
using labrador::CollisionTag;
using labrador::Contact;
using mattmath::RectangleF;
using mattmath::Vector2F;

namespace
{
	constexpr CollisionLayer WORLD = 1u << 0;
	constexpr CollisionLayer MOVER = 1u << 1;

	class Box final : public CollisionObject
	{
	public:
		Box(const RectangleF& rectangle, CollisionLayer layer,
			CollisionMask mask) :
			rectangle_(rectangle), layer_(layer), mask_(mask)
		{
		}

		void update(float /*dt*/) override {}
		void draw(labrador::DrawList& /*list*/) const override {}
		RectangleF bounds() const override { return this->rectangle_; }

		const mattmath::Shape* shape() const override
		{
			return &this->rectangle_;
		}
		CollisionLayer layer() const override { return this->layer_; }
		CollisionMask mask() const override { return this->mask_; }
		CollisionTag tag() const override { return 0; }

		void on_contact(const CollisionObject& /*other*/,
			const Vector2F& /*normal*/, float /*penetration*/) override
		{
		}

		bool for_deletion() const override { return this->for_deletion_; }
		void set_for_deletion(bool value) override
		{
			this->for_deletion_ = value;
		}

	private:
		RectangleF rectangle_;
		CollisionLayer layer_ = 0;
		CollisionMask mask_ = 0;
		bool for_deletion_ = false;
	};

	// A deterministic scatter. std::rand's sequence is not specified across
	// implementations, and a test that only reproduces on one library is not
	// a regression test.
	struct Rng
	{
		uint32_t state = 0x9e3779b9u;

		uint32_t next()
		{
			this->state ^= this->state << 13;
			this->state ^= this->state >> 17;
			this->state ^= this->state << 5;
			return this->state;
		}

		float in(float low, float high)
		{
			const float unit = static_cast<float>(this->next() % 100000u) /
				100000.0f;
			return low + unit * (high - low);
		}
	};

	// Overlapping objects of assorted sizes, which is the case that separates
	// a correct grid from one that only works when nothing straddles a cell.
	std::vector<std::unique_ptr<Box>> scatter(int count, Rng& rng,
		float world, float min_size, float max_size)
	{
		std::vector<std::unique_ptr<Box>> boxes;
		boxes.reserve(static_cast<size_t>(count));
		for (int i = 0; i < count; i++)
		{
			const float w = rng.in(min_size, max_size);
			const float h = rng.in(min_size, max_size);
			const bool mover = (i % 3) == 0;
			boxes.push_back(std::make_unique<Box>(
				RectangleF(rng.in(0.0f, world), rng.in(0.0f, world), w, h),
				mover ? MOVER : WORLD,
				mover ? (WORLD | MOVER) : MOVER));
		}
		return boxes;
	}

	std::vector<CollisionObject*> pointers(
		const std::vector<std::unique_ptr<Box>>& boxes)
	{
		std::vector<CollisionObject*> out;
		out.reserve(boxes.size());
		for (const std::unique_ptr<Box>& box : boxes)
		{
			out.push_back(box.get());
		}
		return out;
	}

	bool same(const std::vector<Contact>& a, const std::vector<Contact>& b)
	{
		if (a.size() != b.size())
		{
			return false;
		}
		for (size_t i = 0; i < a.size(); i++)
		{
			if (a[i].a != b[i].a || a[i].b != b[i].b)
			{
				return false;
			}
			if (a[i].penetration != b[i].penetration)
			{
				return false;
			}
			if (!(a[i].normal == b[i].normal))
			{
				return false;
			}
		}
		return true;
	}
}

TEST_SUITE("BroadPhaseTests")
{
	// THE test. The exhaustive sweep is the specification, so the grid is
	// correct exactly when it produces the same contact list - same pairs,
	// same order, same measurements.
	//
	// Order and not just membership: dispatch re-measures each pair in list
	// order and a response moves things, so a grid that found the same pairs
	// in a different order would resolve a pile of objects differently.
	TEST_CASE("the grid finds exactly what the all-pairs sweep finds")
	{
		Rng rng;

		struct Scenario
		{
			const char* name;
			int count;
			float world;
			float min_size;
			float max_size;
		};

		const Scenario scenarios[] = {
			// Sparse: almost nothing touches.
			{ "sparse", 200, 4000.0f, 10.0f, 20.0f },
			// Dense: nearly everything touches something.
			{ "dense", 200, 300.0f, 10.0f, 20.0f },
			// Mixed sizes, so objects straddle several cells each.
			{ "mixed sizes", 200, 1500.0f, 5.0f, 200.0f },
			// One object far larger than the rest, which is the case the
			// large-object list exists for.
			{ "with a giant", 150, 1000.0f, 5.0f, 30.0f },
			// Small enough to hit the early-out paths.
			{ "tiny", 2, 50.0f, 10.0f, 20.0f },
		};

		for (const Scenario& scenario : scenarios)
		{
			CAPTURE(scenario.name);

			std::vector<std::unique_ptr<Box>> boxes =
				scatter(scenario.count, rng, scenario.world,
					scenario.min_size, scenario.max_size);

			if (std::string(scenario.name) == "with a giant")
			{
				boxes.push_back(std::make_unique<Box>(
					RectangleF(0.0f, 0.0f, 5000.0f, 5000.0f),
					WORLD, MOVER));
			}

			// A few retired objects, because both enumerations have to drop
			// them and the grid drops them in a different place.
			for (size_t i = 0; i < boxes.size(); i += 17)
			{
				boxes[i]->set_for_deletion(true);
			}

			const std::vector<CollisionObject*> objects = pointers(boxes);

			std::vector<Contact> swept;
			find_contacts(objects, swept);

			BroadPhase broad_phase;
			std::vector<Contact> indexed;
			find_contacts(objects, indexed, &broad_phase);

			CHECK(same(swept, indexed));
		}
	}

	TEST_CASE("an empty or single-object scene produces nothing")
	{
		BroadPhase broad_phase;
		std::vector<Contact> contacts;

		const std::vector<CollisionObject*> none;
		find_contacts(none, contacts, &broad_phase);
		CHECK(contacts.empty());

		const Box only(RectangleF(0.0f, 0.0f, 10.0f, 10.0f), WORLD, MOVER);
		std::vector<CollisionObject*> one{ const_cast<Box*>(&only) };
		find_contacts(one, contacts, &broad_phase);
		CHECK(contacts.empty());
	}

	TEST_CASE("objects stacked at one point do not divide by a zero cell")
	{
		// Every extent zero means a mean extent of zero, and a cell size of
		// zero is a division by it in the very first index computation.
		std::vector<std::unique_ptr<Box>> boxes;
		for (int i = 0; i < 8; i++)
		{
			boxes.push_back(std::make_unique<Box>(
				RectangleF(0.0f, 0.0f, 0.0f, 0.0f), WORLD, MOVER));
		}

		BroadPhase broad_phase;
		std::vector<Contact> contacts;
		find_contacts(pointers(boxes), contacts, &broad_phase);

		CHECK(broad_phase.cell_size() > 0.0f);
	}

	TEST_CASE("a null entry in the object list is skipped by both paths")
	{
		std::vector<std::unique_ptr<Box>> boxes;
		boxes.push_back(std::make_unique<Box>(
			RectangleF(0.0f, 0.0f, 10.0f, 10.0f), MOVER, WORLD | MOVER));
		boxes.push_back(std::make_unique<Box>(
			RectangleF(5.0f, 5.0f, 10.0f, 10.0f), WORLD, MOVER));

		std::vector<CollisionObject*> objects{ boxes[0].get(), nullptr,
			boxes[1].get() };

		std::vector<Contact> swept;
		find_contacts(objects, swept);

		BroadPhase broad_phase;
		std::vector<Contact> indexed;
		find_contacts(objects, indexed, &broad_phase);

		CHECK(swept.size() == 1);
		CHECK(same(swept, indexed));
	}
}
