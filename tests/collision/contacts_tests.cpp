#include <doctest/doctest.h>

#include "engine/collision/collision_object.h"
#include "engine/collision/contacts.h"
#include "engine/collision/resolve.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"

#include <algorithm>
#include <vector>

using labrador::CollisionLayer;
using labrador::CollisionMask;
using labrador::CollisionObject;
using labrador::CollisionTag;
using labrador::Contact;
using labrador::dispatch_contacts;
using labrador::find_contacts;
using mattmath::RectangleF;
using mattmath::Vector2F;

namespace
{
	constexpr CollisionLayer PLAYER = 1u << 0;
	constexpr CollisionLayer WALL = 1u << 1;
	constexpr CollisionLayer BULLET = 1u << 2;

	class TestObject : public CollisionObject
	{
	public:
		struct Received
		{
			const CollisionObject* other = nullptr;
			Vector2F normal;
			float penetration = 0.0f;
		};

		TestObject(const RectangleF& rectangle, CollisionLayer layer,
			CollisionMask mask, CollisionTag tag = 0) :
			rectangle_(rectangle), layer_(layer), mask_(mask), tag_(tag)
		{
		}

		void update(float /*dt*/) override {}
		void draw(labrador::DrawList& /*draw_list*/) const override {}
		RectangleF bounds() const override { return this->rectangle_; }

		const mattmath::Shape* shape() const override { return &this->rectangle_; }
		CollisionLayer layer() const override { return this->layer_; }
		CollisionMask mask() const override { return this->mask_; }
		CollisionTag tag() const override { return this->tag_; }

		bool for_deletion() const override { return this->for_deletion_; }
		void set_for_deletion(bool for_deletion) override
		{
			this->for_deletion_ = for_deletion;
		}

		void on_contact(const CollisionObject& other, const Vector2F& normal,
			float penetration) override
		{
			this->received_.push_back(Received{ &other, normal, penetration });
			if (this->retire_on_contact_)
			{
				this->for_deletion_ = true;
			}
			if (this->separate_on_contact_)
			{
				this->rectangle_.offset(labrador::separation(normal, penetration));
			}
		}

		const std::vector<Received>& received() const { return this->received_; }
		const RectangleF& rectangle() const { return this->rectangle_; }
		void retire_on_contact() { this->retire_on_contact_ = true; }
		void separate_on_contact() { this->separate_on_contact_ = true; }

	private:
		RectangleF rectangle_;
		CollisionLayer layer_ = 0;
		CollisionMask mask_ = 0;
		CollisionTag tag_ = 0;
		bool for_deletion_ = false;
		bool retire_on_contact_ = false;
		bool separate_on_contact_ = false;
		std::vector<Received> received_;
	};

	bool names(const Contact& contact, const CollisionObject& x,
		const CollisionObject& y)
	{
		return (contact.a == &x && contact.b == &y) ||
			(contact.a == &y && contact.b == &x);
	}

	// Membership, not position: the order of `contacts` is unspecified and
	// follows whatever the broad phase's sort produced. Asserting a pair is
	// present, together with the total count, still pins "every pair once and
	// no pair twice" without pinning an order nothing promises.
	bool contains_pair(const std::vector<Contact>& contacts,
		const CollisionObject& x, const CollisionObject& y)
	{
		return std::any_of(contacts.begin(), contacts.end(),
			[&x, &y](const Contact& contact) { return names(contact, x, y); });
	}
}

TEST_CASE("an overlapping pair is reported once, not twice and not against itself")
{
	// Three objects all in the same place. Three pairs, so three contacts -
	// the nested loops this replaces produced six, plus three self-pairs that
	// only an identity check kept out.
	TestObject a(RectangleF(0.0f, 0.0f, 10.0f, 10.0f), PLAYER, WALL | BULLET);
	TestObject b(RectangleF(2.0f, 2.0f, 10.0f, 10.0f), WALL, PLAYER | BULLET);
	TestObject c(RectangleF(4.0f, 4.0f, 10.0f, 10.0f), BULLET, PLAYER | WALL);

	CollisionObject* objects[] = { &a, &b, &c };
	std::vector<Contact> contacts;
	find_contacts(objects, contacts);

	REQUIRE(contacts.size() == 3);
	CHECK(contains_pair(contacts, a, b));
	CHECK(contains_pair(contacts, a, c));
	CHECK(contains_pair(contacts, b, c));
}

TEST_CASE("either side can veto a pair")
{
	// A responds to walls; B is a wall that responds to nothing. The filter
	// it replaces asked one participant only, so which answer counted came
	// down to which of two nested loops reached the pair first.
	TestObject a(RectangleF(0.0f, 0.0f, 10.0f, 10.0f), PLAYER, WALL);
	TestObject b(RectangleF(2.0f, 2.0f, 10.0f, 10.0f), WALL, 0);

	CollisionObject* objects[] = { &a, &b };
	std::vector<Contact> contacts;
	find_contacts(objects, contacts);

	CHECK(contacts.empty());
}

TEST_CASE("objects in unrelated layers overlapping in space are not a contact")
{
	TestObject a(RectangleF(0.0f, 0.0f, 10.0f, 10.0f), PLAYER, WALL);
	TestObject b(RectangleF(2.0f, 2.0f, 10.0f, 10.0f), BULLET, WALL);

	CollisionObject* objects[] = { &a, &b };
	std::vector<Contact> contacts;
	find_contacts(objects, contacts);

	CHECK(contacts.empty());
}

TEST_CASE("an object already flagged for deletion takes part in nothing")
{
	TestObject a(RectangleF(0.0f, 0.0f, 10.0f, 10.0f), PLAYER, WALL);
	TestObject b(RectangleF(2.0f, 2.0f, 10.0f, 10.0f), WALL, PLAYER);
	b.set_for_deletion(true);

	CollisionObject* objects[] = { &a, &b };
	std::vector<Contact> contacts;
	find_contacts(objects, contacts);

	CHECK(contacts.empty());
}

TEST_CASE("the contact vector is cleared before it is filled")
{
	TestObject a(RectangleF(0.0f, 0.0f, 10.0f, 10.0f), PLAYER, WALL);
	TestObject b(RectangleF(100.0f, 100.0f, 10.0f, 10.0f), WALL, PLAYER);

	std::vector<Contact> contacts(7);
	CollisionObject* objects[] = { &a, &b };
	find_contacts(objects, contacts);

	CHECK(contacts.empty());
}

TEST_CASE("both participants are told, once each, with opposite normals")
{
	// The bug this closes: the dispatch fired both objects' responses off one
	// object's predicate, so a player's response to a projectile ran because
	// the *projectile* thought they were touching - and then the reversed
	// pass ran both again.
	TestObject player(RectangleF(0.0f, 0.0f, 10.0f, 100.0f), PLAYER, WALL);
	TestObject wall(RectangleF(8.0f, 0.0f, 100.0f, 100.0f), WALL, PLAYER);

	CollisionObject* objects[] = { &player, &wall };
	std::vector<Contact> contacts;
	find_contacts(objects, contacts);
	dispatch_contacts(contacts);

	REQUIRE(player.received().size() == 1);
	REQUIRE(wall.received().size() == 1);

	CHECK(player.received()[0].other == &wall);
	CHECK(wall.received()[0].other == &player);

	CHECK(player.received()[0].normal == Vector2F::DIRECTION_RIGHT);
	CHECK(wall.received()[0].normal == Vector2F::DIRECTION_LEFT);

	CHECK(player.received()[0].penetration == doctest::Approx(2.0f));
	CHECK(wall.received()[0].penetration == doctest::Approx(2.0f));
}

TEST_CASE("a contact an earlier response already separated is dropped")
{
	// A player standing on the seam between two floor tiles: two contacts, the
	// same depth on each. Separating from the first ends the second, and
	// applying the depth that was true before it would push them out twice -
	// a visible pop at every tile seam, which is why the depth is measured
	// again at dispatch rather than taken from the list.
	TestObject player(RectangleF(95.0f, 100.0f, 20.0f, 100.0f), PLAYER, WALL);
	TestObject left(RectangleF(0.0f, 196.0f, 100.0f, 40.0f), WALL, PLAYER);
	TestObject right(RectangleF(100.0f, 196.0f, 100.0f, 40.0f), WALL, PLAYER);
	player.separate_on_contact();

	CollisionObject* objects[] = { &player, &left, &right };
	std::vector<Contact> contacts;
	find_contacts(objects, contacts);

	REQUIRE(contacts.size() == 2);

	dispatch_contacts(contacts);

	// Pushed up by the 4 they overlapped, once - not by 8.
	CHECK(player.rectangle().top() == doctest::Approx(96.0f));
	CHECK(player.received().size() == 1);
}

TEST_CASE("a contact is dropped when an earlier response retired a participant")
{
	// A bullet that hits a wall does not go on to hit the player standing
	// behind it. That rule used to be a `continue` inside the sweep, so
	// whether it held depended on which nested loop reached the pair; here it
	// holds for both sides of every pair.
	TestObject bullet(RectangleF(50.0f, 50.0f, 10.0f, 10.0f),
		BULLET, WALL | PLAYER);
	TestObject wall(RectangleF(0.0f, 0.0f, 100.0f, 55.0f), WALL, BULLET);
	TestObject player(RectangleF(0.0f, 58.0f, 100.0f, 50.0f), PLAYER, BULLET);
	bullet.retire_on_contact();

	CollisionObject* objects[] = { &bullet, &wall, &player };
	std::vector<Contact> contacts;
	find_contacts(objects, contacts);

	// Both overlaps are found: the sweep measures the frame as it stood at
	// the start of it, and the list is a value nothing has responded to yet.
	REQUIRE(contacts.size() == 2);

	dispatch_contacts(contacts);

	CHECK(bullet.received().size() == 1);
	CHECK(wall.received().size() == 1);
	CHECK(player.received().empty());
}
