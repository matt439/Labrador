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

	// One sweep's outcome: what it cost, and what it found. The contact count
	// is not part of SweepCounts because it is already contacts.size() - this
	// exists so a helper can hand back both without returning the vector.
	struct SweepResult
	{
		SweepCounts counts;
		size_t contacts = 0;
	};

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

	// All-pairs would enumerate 1,148,370 here. The sweep enumerates 66,210:
	// each column of tiles reaches the rest of its own column and all of the
	// next, and stops. Exact, and derived - 49 columns contributing
	// 435 + 900 each, the last column 435, plus 360 bullet-tile pairs.
	CHECK(counts.pairs_enumerated == 66210);
	CHECK(counts.pairs_enumerated * 17 < every_pair_of(
		static_cast<long long>(objects.size())));

	// The prize. Every bullet used to be measured against all 1500 tiles; it
	// is now measured against the 30 in its own column, because nothing else
	// begins before its 8-unit interval ends. 18,048 box tests down to 360.
	CHECK(counts.bound_tests ==
		static_cast<long long>(TILE_ROWS) * BULLET_COUNT);

	// Worth noticing rather than fixing: of the 66,210 pairs the sweep offers,
	// 65,850 are tile against tile and die on the layer filter. The sweep is
	// re-deriving something the layer bits already knew. A broad phase that
	// indexed only objects that can collide with something would start from
	// 360, and that is a later question - it needs an index per layer group,
	// which is a design decision and not a loop change.
	CHECK(counts.pairs_enumerated - counts.bound_tests == 65850);

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

	// Zero. Not "fewer" - zero. Every object's interval ends 160 units before
	// the next one begins, so the sweep's break fires on the first comparison
	// every time and no pair is ever offered to a filter. This is the case
	// all-pairs handled worst: 319,600 box tests to discover that nothing in
	// the scene touches anything.
	CHECK(counts.pairs_enumerated == 0);
	CHECK(counts.bound_tests == 0);
	CHECK(counts.narrow_tests == 0);
	CHECK(contacts.empty());

	// The comparison the sweep replaced, kept as an assertion so the scale of
	// it stays visible rather than becoming a story about the old days.
	CHECK(every_pair_of(COUNT) == 319600);
}

TEST_CASE("benchmark: doubling a dense scene doubles the work, it does not quadruple it")
{
	// A scene the sweep cannot simply prune away: objects 40 wide every 20
	// units, so each one genuinely overlaps its two forward neighbours and
	// stops. That is the honest test of the exponent - the scattered case
	// above collapses to zero, which proves the break works but measures
	// nothing about how the cost grows.
	//
	// Twice the objects for twice the work. All-pairs gave twice the objects
	// for four times the work, and that change of exponent is the whole
	// reason a broad phase exists.
	const auto sweep = [](int count)
	{
		std::vector<BenchObject> storage;
		storage.reserve(count);
		for (int i = 0; i < count; i++)
		{
			const float x = i * 20.0f;
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
		return SweepResult{ counts, contacts.size() };
	};

	// Not `small` and `large`: the Windows SDK defines `small` as `char`.
	const SweepResult half = sweep(200);
	const SweepResult full = sweep(400);
	const SweepCounts& half_scene = half.counts;
	const SweepCounts& full_scene = full.counts;

	MESSAGE("200 objects: " << half_scene.bound_tests
		<< " bound tests; 400 objects: " << full_scene.bound_tests
		<< " bound tests");

	// Each object reaches its next two neighbours and no further, so the count
	// is 2n - 3 exactly: (n - 1) adjacent pairs plus (n - 2) once-removed.
	CHECK(half_scene.bound_tests == 2 * 200 - 3);
	CHECK(full_scene.bound_tests == 2 * 400 - 3);

	const double ratio = static_cast<double>(full_scene.bound_tests)
		/ static_cast<double>(half_scene.bound_tests);
	CHECK(ratio == doctest::Approx(2.0).epsilon(0.02));

	// And it is linear against the object count, not against the pair count:
	// 797 box tests where all-pairs would have run 79,800.
	CHECK(full_scene.bound_tests * 100 < every_pair_of(400));

	// The once-removed neighbours touch exactly, at a shared edge. They pass
	// the bounding-box filter, which is closed, and are then rejected by
	// narrow_phase, which is open - so touching is not overlapping, and only
	// the n - 1 genuinely overlapping pairs become contacts.
	CHECK(full_scene.narrow_tests == 2 * 400 - 3);
	CHECK(full.contacts == 400 - 1);
}
