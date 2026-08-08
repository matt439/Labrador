// Throughput benchmarks for the contact sweep.
//
// PHILOSOPHY (Performance) asks that benchmarks pin throughput the way tests
// pin behaviour, and that a throughput regression be a defect rather than a
// curiosity. These are that instrument, and they are deliberately not timers.
//
// What they measure is work, in Ericson's terms (Real-Time Collision
// Detection, 6.1.2): the number of bounding-volume tests and primitive tests a
// query performs. A millisecond figure cannot be asserted on - it moves with
// the machine, the build type and what else is running - but "this scene
// enumerates 1,148,370 pairs" is an integer, and it is the same integer on
// every machine. The per-test costs are what a profiler is for; these are the
// half of the product the algorithm chooses.
//
// The numbers below are exact for the sweep as it stands today, which is
// all-pairs enumeration behind two cheap filters. That is the point. When a
// broad phase lands, these assertions fail, and whoever landed it writes the
// new numbers down - which is the only way "we made it faster" ever becomes a
// fact the build can check.

#include <doctest/doctest.h>

#include "engine/collision/collision_object.h"
#include "engine/collision/contacts.h"

#include <string>
#include <vector>

using artattack::CollisionLayer;
using artattack::CollisionMask;
using artattack::CollisionObject;
using artattack::CollisionTag;
using artattack::Contact;
using artattack::find_contacts;
using artattack::SweepCounts;
using mattmath::RectangleF;
using mattmath::Vector2F;

namespace
{
	constexpr CollisionLayer TILE = 1u << 0;
	constexpr CollisionLayer PLAYER = 1u << 1;
	constexpr CollisionLayer BULLET = 1u << 2;
	constexpr CollisionLayer LOOSE = 1u << 3;

	class BenchObject : public CollisionObject
	{
	public:
		BenchObject(const RectangleF& rectangle, CollisionLayer layer,
			CollisionMask mask) :
			rectangle_(rectangle), layer_(layer), mask_(mask)
		{
		}

		void update(float /*dt*/) override {}
		void draw(artattack::DrawList& /*draw_list*/) const override {}
		RectangleF bounds() const override { return this->rectangle_; }

		const mattmath::Shape* shape() const override { return &this->rectangle_; }
		CollisionLayer layer() const override { return this->layer_; }
		CollisionMask mask() const override { return this->mask_; }
		CollisionTag tag() const override { return 0; }

		bool for_deletion() const override { return false; }
		void on_contact(const CollisionObject& /*other*/,
			const Vector2F& /*normal*/, float /*penetration*/) override {}

	private:
		RectangleF rectangle_;
		CollisionLayer layer_ = 0;
		CollisionMask mask_ = 0;
	};

	// Every pair of n objects, counted once. The law the sweep obeys today,
	// written as arithmetic so the assertions below say why they hold.
	constexpr long long every_pair_of(long long n)
	{
		return n * (n - 1) / 2;
	}

	// `scene` is a std::string, not a const char*: doctest's MessageBuilder
	// takes a char pointer through its bool overload and prints "1". String
	// literals are arrays and stream correctly, which is what makes the bug
	// look like it is not there.
	void report(const std::string& scene, long long objects,
		const SweepCounts& counts, size_t contacts)
	{
		MESSAGE(scene << ": " << objects << " objects | pairs enumerated "
			<< counts.pairs_enumerated << " | bound tests " << counts.bound_tests
			<< " | narrow tests " << counts.narrow_tests << " | contacts "
			<< contacts);
	}
}

TEST_CASE("benchmark: a paint-tile grid with players and projectiles")
{
	// The shape of a real level: a large static grid that cannot collide with
	// itself, and a handful of movers that can.
	constexpr int TILE_COLUMNS = 50;
	constexpr int TILE_ROWS = 30;
	constexpr int TILE_COUNT = TILE_COLUMNS * TILE_ROWS;
	constexpr int PLAYER_COUNT = 4;
	constexpr int BULLET_COUNT = 12;
	constexpr float TILE_SIZE = 40.0f;

	std::vector<BenchObject> storage;
	storage.reserve(TILE_COUNT + PLAYER_COUNT + BULLET_COUNT);

	for (int row = 0; row < TILE_ROWS; row++)
	{
		for (int column = 0; column < TILE_COLUMNS; column++)
		{
			// RectangleF is (x, y, width, height) - position and size, not
			// two corners.
			storage.emplace_back(
				RectangleF(column * TILE_SIZE, row * TILE_SIZE,
					TILE_SIZE, TILE_SIZE),
				TILE, BULLET);
		}
	}

	// Parked off the grid, so no player overlaps anything.
	for (int i = 0; i < PLAYER_COUNT; i++)
	{
		const float x = 4000.0f + i * 300.0f;
		storage.emplace_back(RectangleF(x, 0.0f, 52.0f, 120.0f),
			PLAYER, BULLET);
	}

	// Each bullet sits wholly inside one distinct tile, so exactly one
	// bounding-box test per bullet can survive.
	for (int i = 0; i < BULLET_COUNT; i++)
	{
		const float centre_x = (i * 3 + 1) * TILE_SIZE + TILE_SIZE / 2.0f;
		const float centre_y = (i * 2 + 1) * TILE_SIZE + TILE_SIZE / 2.0f;
		storage.emplace_back(
			RectangleF(centre_x - 4.0f, centre_y - 4.0f, 8.0f, 8.0f),
			BULLET, TILE | PLAYER);
	}

	std::vector<CollisionObject*> objects;
	objects.reserve(storage.size());
	for (BenchObject& object : storage)
	{
		objects.push_back(&object);
	}

	std::vector<Contact> contacts;
	SweepCounts counts;
	find_contacts(objects, contacts, counts);

	report("tile grid", static_cast<long long>(objects.size()), counts,
		contacts.size());

	// Quadratic in the object count, and blind to where anything is.
	CHECK(counts.pairs_enumerated ==
		every_pair_of(static_cast<long long>(objects.size())));

	// The layer filter is already a broad phase for the static half: no tile
	// can collide with another tile, and that alone removes 98% of the pairs.
	// What it cannot remove is the reason a broad phase is still wanted -
	// every bullet is measured against all 1500 tiles, though it can only
	// reach the four it is standing on.
	CHECK(counts.bound_tests ==
		static_cast<long long>(TILE_COUNT) * BULLET_COUNT
		+ static_cast<long long>(PLAYER_COUNT) * BULLET_COUNT);
	CHECK(counts.bound_tests * 60 < counts.pairs_enumerated);

	// One tile per bullet survives the box test; nothing else is near anything.
	CHECK(counts.narrow_tests == BULLET_COUNT);
	CHECK(contacts.size() == BULLET_COUNT);
}

TEST_CASE("benchmark: mutually eligible objects, none of them touching")
{
	// The case with no filter to hide behind: every object can collide with
	// every other, so the layer test prunes nothing and all-pairs enumeration
	// is paid in full. This is the worst case the sweep has, and the one a
	// broad phase exists to answer - the objects are spread far enough apart
	// that the true answer is no contacts at all.
	constexpr int COUNT = 800;
	constexpr float SPACING = 200.0f;

	std::vector<BenchObject> storage;
	storage.reserve(COUNT);
	for (int i = 0; i < COUNT; i++)
	{
		const float x = i * SPACING;
		storage.emplace_back(RectangleF(x, 0.0f, 40.0f, 40.0f),
			LOOSE, LOOSE);
	}

	std::vector<CollisionObject*> objects;
	objects.reserve(storage.size());
	for (BenchObject& object : storage)
	{
		objects.push_back(&object);
	}

	std::vector<Contact> contacts;
	SweepCounts counts;
	find_contacts(objects, contacts, counts);

	report("scattered", COUNT, counts, contacts.size());

	CHECK(counts.pairs_enumerated == every_pair_of(COUNT));

	// Nothing is filtered, so a bounding box is tested for every pair: 319,600
	// box tests to discover that nothing in the scene touches anything.
	CHECK(counts.bound_tests == every_pair_of(COUNT));
	CHECK(counts.narrow_tests == 0);
	CHECK(contacts.empty());
}

TEST_CASE("benchmark: the cost is quadratic, and doubling the scene quadruples it")
{
	// Stated as a test rather than left as folklore. Twice the objects for
	// four times the work is the property a broad phase changes; when one
	// lands, this ratio drops and this assertion is where that shows up.
	const auto sweep = [](int count)
	{
		std::vector<BenchObject> storage;
		storage.reserve(count);
		for (int i = 0; i < count; i++)
		{
			const float x = i * 200.0f;
			storage.emplace_back(RectangleF(x, 0.0f, x + 40.0f, 40.0f),
				LOOSE, LOOSE);
		}

		std::vector<CollisionObject*> objects;
		objects.reserve(storage.size());
		for (BenchObject& object : storage)
		{
			objects.push_back(&object);
		}

		std::vector<Contact> contacts;
		SweepCounts counts;
		find_contacts(objects, contacts, counts);
		return counts;
	};

	// Not `small` and `large`: the Windows SDK defines `small` as `char`.
	const SweepCounts half_scene = sweep(200);
	const SweepCounts full_scene = sweep(400);

	MESSAGE("200 objects: " << half_scene.bound_tests
		<< " bound tests; 400 objects: " << full_scene.bound_tests
		<< " bound tests");

	CHECK(half_scene.bound_tests == every_pair_of(200));
	CHECK(full_scene.bound_tests == every_pair_of(400));

	const double ratio = static_cast<double>(full_scene.bound_tests)
		/ static_cast<double>(half_scene.bound_tests);
	CHECK(ratio == doctest::Approx(4.0).epsilon(0.02));
}
