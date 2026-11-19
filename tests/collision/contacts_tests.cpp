#include <doctest/doctest.h>

#include "engine/collision/collision_object.h"
#include "engine/collision/broad_phase.h"
#include "engine/collision/contacts.h"
#include "engine/collision/resolve.h"
#include "engine/math/circle.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/vector2f.h"
#include "tests/collision/stub_collider.h"

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

using labrador::CollisionLayer;
using labrador::CollisionObject;
using labrador::Contact;
using labrador::dispatch_contacts;
using labrador::find_contacts;
using labrador::RectangleF;
using labrador::Vector2F;

namespace
{
	constexpr CollisionLayer player_layer = 1u << 0;
	constexpr CollisionLayer wall_layer = 1u << 1;
	constexpr CollisionLayer bullet_layer = 1u << 2;

	struct DispatchEvent
	{
		labrador::CollisionTag receiver;
		labrador::CollisionTag other;
		Vector2F normal;
		float penetration;
	};

	class DispatchCollider final : public StubCollider
	{
	public:
		using Response = std::function<void(DispatchCollider&, CollisionObject&)>;

		DispatchCollider(labrador::CollisionTag tag, float x,
			std::vector<DispatchEvent>* events) :
			StubCollider(RectangleF(x, 0.0f, 10.0f, 10.0f), 1u, 1u, tag),
			events_(events)
		{
		}

		void on_contact(CollisionObject& other, const Vector2F& normal,
			float penetration) override
		{
			this->events_->push_back({ this->tag(), other.tag(), normal, penetration });
			if (this->response_)
			{
				this->response_(*this, other);
			}
		}

		void set_response(Response response) { this->response_ = std::move(response); }
		void set_x(float x) { this->rectangle_.x = x; }
		void set_value(int value) { this->value_ = value; }
		int value() const { return this->value_; }

	private:
		std::vector<DispatchEvent>* events_;
		Response response_;
		int value_ = 0;
	};

	using DispatchPair = std::pair<labrador::CollisionTag, labrador::CollisionTag>;

	std::vector<DispatchPair> dispatch_order(const std::vector<DispatchEvent>& events)
	{
		std::vector<DispatchPair> result;
		for (const DispatchEvent& event : events)
		{
			result.emplace_back(event.receiver, event.other);
		}
		return result;
	}

	// The shared stub, plus a log of every contact it receives and two
	// optional responses to one: retiring, and moving out of the overlap.
	class TestObject : public StubCollider
	{
	public:
		struct Received
		{
			const CollisionObject* other = nullptr;
			Vector2F normal;
			float penetration = 0.0f;
		};

		using StubCollider::StubCollider;

		void on_contact(CollisionObject& other, const Vector2F& normal,
			float penetration) override
		{
			this->received_.push_back(Received{ &other, normal, penetration });
			if (this->retire_on_contact_)
			{
				this->set_for_deletion(true);
			}
			if (this->separate_on_contact_)
			{
				this->rectangle_.offset(labrador::separation(normal, penetration));
			}
		}

		const std::vector<Received>& received() const { return this->received_; }
		void retire_on_contact() { this->retire_on_contact_ = true; }
		void separate_on_contact() { this->separate_on_contact_ = true; }

	private:
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

	// Membership assertions keep the shape/filter cases focused. The dispatch
	// cases below separately pin input order for both enumerations.
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
	// not six from visiting each pair from both ends, and no self-pairs.
	TestObject a(RectangleF(0.0f, 0.0f, 10.0f, 10.0f), player_layer, wall_layer | bullet_layer);
	TestObject b(RectangleF(2.0f, 2.0f, 10.0f, 10.0f), wall_layer, player_layer | bullet_layer);
	TestObject c(RectangleF(4.0f, 4.0f, 10.0f, 10.0f), bullet_layer, player_layer | wall_layer);

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
	// A responds to walls; B is a wall that responds to nothing. Asking one
	// participant only would make the answer depend on which of the two the
	// enumeration reached first.
	TestObject a(RectangleF(0.0f, 0.0f, 10.0f, 10.0f), player_layer, wall_layer);
	TestObject b(RectangleF(2.0f, 2.0f, 10.0f, 10.0f), wall_layer, 0);

	CollisionObject* objects[] = { &a, &b };
	std::vector<Contact> contacts;
	find_contacts(objects, contacts);

	CHECK(contacts.empty());
}

TEST_CASE("objects in unrelated layers overlapping in space are not a contact")
{
	TestObject a(RectangleF(0.0f, 0.0f, 10.0f, 10.0f), player_layer, wall_layer);
	TestObject b(RectangleF(2.0f, 2.0f, 10.0f, 10.0f), bullet_layer, wall_layer);

	CollisionObject* objects[] = { &a, &b };
	std::vector<Contact> contacts;
	find_contacts(objects, contacts);

	CHECK(contacts.empty());
}

TEST_CASE("an object already flagged for deletion takes part in nothing")
{
	TestObject a(RectangleF(0.0f, 0.0f, 10.0f, 10.0f), player_layer, wall_layer);
	TestObject b(RectangleF(2.0f, 2.0f, 10.0f, 10.0f), wall_layer, player_layer);
	b.set_for_deletion(true);

	CollisionObject* objects[] = { &a, &b };
	std::vector<Contact> contacts;
	find_contacts(objects, contacts);

	CHECK(contacts.empty());
}

TEST_CASE("the contact vector is cleared before it is filled")
{
	TestObject a(RectangleF(0.0f, 0.0f, 10.0f, 10.0f), player_layer, wall_layer);
	TestObject b(RectangleF(100.0f, 100.0f, 10.0f, 10.0f), wall_layer, player_layer);

	std::vector<Contact> contacts(7);
	CollisionObject* objects[] = { &a, &b };
	find_contacts(objects, contacts);

	CHECK(contacts.empty());
}

TEST_CASE("both participants are told, once each, with opposite normals")
{
	// Each object's response runs once, for the pair the narrow phase
	// measured, and the two normals are opposite. Firing both responses off
	// one object's predicate would run a player's response because the
	// *projectile* thought they were touching.
	TestObject player(RectangleF(0.0f, 0.0f, 10.0f, 100.0f), player_layer, wall_layer);
	TestObject wall(RectangleF(8.0f, 0.0f, 100.0f, 100.0f), wall_layer, player_layer);

	CollisionObject* objects[] = { &player, &wall };
	std::vector<Contact> contacts;
	find_contacts(objects, contacts);
	dispatch_contacts(contacts);

	REQUIRE(player.received().size() == 1);
	REQUIRE(wall.received().size() == 1);

	CHECK(player.received()[0].other == &wall);
	CHECK(wall.received()[0].other == &player);

	CHECK(player.received()[0].normal == Vector2F::direction_right);
	CHECK(wall.received()[0].normal == Vector2F::direction_left);

	CHECK(player.received()[0].penetration == doctest::Approx(2.0f));
	CHECK(wall.received()[0].penetration == doctest::Approx(2.0f));
}

TEST_CASE("a contact an earlier response already separated is dropped")
{
	// A player standing on the seam between two floor tiles: two contacts, the
	// same depth on each. Separating from the first ends the second, and
	// applying the depth measured before it would push them out twice -
	// a visible pop at every tile seam, which is why the depth is measured
	// again at dispatch rather than taken from the list.
	TestObject player(RectangleF(95.0f, 100.0f, 20.0f, 100.0f), player_layer, wall_layer);
	TestObject left(RectangleF(0.0f, 196.0f, 100.0f, 40.0f), wall_layer, player_layer);
	TestObject right(RectangleF(100.0f, 196.0f, 100.0f, 40.0f), wall_layer, player_layer);
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
	// behind it. As a `continue` inside the sweep the rule holds or not
	// depending on which nested loop reached the pair; here it holds for both
	// sides of every pair.
	TestObject bullet(RectangleF(50.0f, 50.0f, 10.0f, 10.0f),
		bullet_layer, wall_layer | player_layer);
	TestObject wall(RectangleF(0.0f, 0.0f, 100.0f, 55.0f), wall_layer, bullet_layer);
	TestObject player(RectangleF(0.0f, 58.0f, 100.0f, 50.0f), player_layer, bullet_layer);
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

TEST_CASE("overlapping AABBs do not manufacture contact between disjoint thin polygons")
{
	class ThinCollider : public TestObject
	{
	public:
		explicit ThinCollider(const labrador::RectangleRotated& shape) :
			TestObject(shape.bounding_box(), player_layer, player_layer), shape_(shape) {}
		const labrador::Shape* shape() const override { return &this->shape_; }
	private:
		labrador::RectangleRotated shape_;
	};

	const Vector2F x_axis(0.70710678f, 0.70710678f);
	const Vector2F y_axis(-0.70710678f, 0.70710678f);
	const Vector2F half_extents(0.0001f, 1.0f);
	ThinCollider first(labrador::RectangleRotated(Vector2F(0.0f, 0.0f),
		x_axis, y_axis, half_extents));
	ThinCollider second(labrador::RectangleRotated(y_axis * 2.0001f,
		x_axis, y_axis, half_extents));
	REQUIRE(first.bounds().intersects(second.bounds()));
	REQUIRE(Vector2F::dot(second.shape()->center() - first.shape()->center(), y_axis) > 2.0f);

	labrador::BroadPhase broad_phase;
	std::vector<CollisionObject*> objects{ &first, &second };
	std::vector<Contact> contacts;
	find_contacts(objects, contacts, &broad_phase);
	CHECK(contacts.empty());
}

TEST_CASE("circles and polygons share one sweep, and each pair is measured by shape")
{
	// A round object among boxes: the broad phase sees only bounds, and the
	// narrow phase answers each pair for the shapes it actually has.
	class RoundObject : public TestObject
	{
	public:
		explicit RoundObject(const labrador::Circle& circle) :
			TestObject(circle.bounding_box(), player_layer,
				wall_layer | player_layer), circle_(circle) {}
		const labrador::Shape* shape() const override { return &this->circle_; }
	private:
		labrador::Circle circle_;
	};

	RoundObject ball(labrador::Circle(Vector2F(0.0f, 0.0f), 5.0f));
	// Overlaps the ball's face by one unit.
	TestObject floor(RectangleF(-20.0f, 4.0f, 40.0f, 10.0f), wall_layer, player_layer);
	// Overlaps the ball's bounding box at its corner, but not the ball.
	TestObject corner(RectangleF(4.0f, -9.0f, 5.0f, 5.0f), wall_layer, player_layer);
	// Overlaps the ball along the line between the centres, and rests on
	// the floor without overlapping it.
	RoundObject other(labrador::Circle(Vector2F(-8.0f, 0.0f), 4.0f));
	REQUIRE(ball.bounds().intersects(corner.bounds()));

	labrador::BroadPhase broad_phase;
	std::vector<CollisionObject*> objects{ &ball, &floor, &corner, &other };
	std::vector<Contact> contacts;
	find_contacts(objects, contacts, &broad_phase);

	CHECK(contacts.size() == 2);
	CHECK(contains_pair(contacts, ball, floor));
	CHECK(contains_pair(contacts, ball, other));
	CHECK_FALSE(contains_pair(contacts, other, floor));
	CHECK_FALSE(contains_pair(contacts, ball, corner));

	dispatch_contacts(contacts);
	const auto from = [](const TestObject& object, const CollisionObject& other_object)
	{
		for (const TestObject::Received& received : object.received())
		{
			if (received.other == &other_object)
			{
				return received;
			}
		}
		FAIL("no contact received");
		return TestObject::Received{};
	};

	CHECK(from(ball, floor).normal.x == doctest::Approx(0.0f));
	CHECK(from(ball, floor).normal.y == doctest::Approx(1.0f));
	CHECK(from(ball, floor).penetration == doctest::Approx(1.0f));
	CHECK(from(floor, ball).normal.y == doctest::Approx(-1.0f));

	CHECK(from(ball, other).normal.x == doctest::Approx(-1.0f));
	CHECK(from(ball, other).penetration == doctest::Approx(1.0f));
	CHECK(from(other, ball).normal.x == doctest::Approx(1.0f));
}

TEST_CASE("both enumerations dispatch pairs and halves in input order")
{
	for (bool indexed : { false, true })
	{
		CAPTURE(indexed);
		std::vector<DispatchEvent> events;
		DispatchCollider a(30, 0.0f, &events);
		DispatchCollider b(10, 2.0f, &events);
		DispatchCollider c(20, 4.0f, &events);
		DispatchCollider retired(5, 1.0f, &events);
		retired.set_for_deletion(true);
		CollisionObject* objects[] = { &a, nullptr, &retired, &b, &c };
		labrador::BroadPhase broad_phase;
		std::vector<Contact> contacts;
		find_contacts(objects, contacts, indexed ? &broad_phase : nullptr);
		REQUIRE(contacts.size() == 3);
		CHECK(contacts[0].a == &a);
		CHECK(contacts[0].b == &b);
		CHECK(contacts[1].a == &a);
		CHECK(contacts[1].b == &c);
		CHECK(contacts[2].a == &b);
		CHECK(contacts[2].b == &c);
		dispatch_contacts(contacts);
		const std::vector<DispatchPair> expected{
			{ 30, 10 }, { 10, 30 }, { 30, 20 }, { 20, 30 }, { 10, 20 }, { 20, 10 } };
		CHECK(dispatch_order(events) == expected);
	}
}

TEST_CASE("each callback half may mutate its counterpart directly")
{
	std::vector<DispatchEvent> events;
	DispatchCollider a(1, 0.0f, &events);
	DispatchCollider b(2, 2.0f, &events);
	a.set_response([](DispatchCollider&, CollisionObject& other)
		{
			static_cast<DispatchCollider&>(other).set_value(11);
		});
	b.set_response([](DispatchCollider& self, CollisionObject& other)
		{
			CHECK(self.value() == 11);
			static_cast<DispatchCollider&>(other).set_value(22);
		});
	CollisionObject* objects[] = { &a, &b };
	std::vector<Contact> contacts;
	find_contacts(objects, contacts);
	dispatch_contacts(contacts);
	CHECK(a.value() == 22);
	CHECK(b.value() == 11);
	REQUIRE(events.size() == 2);
	CHECK(events[0].normal == -events[1].normal);
	CHECK(events[0].penetration == events[1].penetration);
}

TEST_CASE("a pair reuses one measurement while later pairs see mutated geometry")
{
	for (bool indexed : { false, true })
	{
		CAPTURE(indexed);
		std::vector<DispatchEvent> events;
		DispatchCollider a(1, 0.0f, &events);
		DispatchCollider b(2, 2.0f, &events);
		DispatchCollider c(3, 4.0f, &events);
		a.set_response([](DispatchCollider&, CollisionObject& other)
			{
				if (other.tag() == 2u)
				{
					static_cast<DispatchCollider&>(other).set_x(7.0f);
				}
			});
		CollisionObject* objects[] = { &a, &b, &c };
		labrador::BroadPhase broad_phase;
		std::vector<Contact> contacts;
		find_contacts(objects, contacts, indexed ? &broad_phase : nullptr);
		REQUIRE(contacts.size() == 3);
		dispatch_contacts(contacts);
		REQUIRE(events.size() == 6);
		// AB was measured at B.x=2. Moving B to 7 would change depth to 3,
		// but its second half still receives the original 8, facing A.
		CHECK(events[0].normal == Vector2F(1.0f, 0.0f));
		CHECK(events[0].penetration == 8.0f);
		CHECK(events[1].normal == Vector2F(-1.0f, 0.0f));
		CHECK(events[1].penetration == 8.0f);
		// BC is reached later: its direction and depth now differ from the
		// pre-response snapshot (B.x=2, normal +x, depth 8).
		CHECK(events[4].receiver == 2u);
		CHECK(events[4].other == 3u);
		CHECK(events[4].normal == Vector2F(-1.0f, 0.0f));
		CHECK(events[4].penetration == 7.0f);
		CHECK(events[5].normal == -events[4].normal);
		CHECK(events[5].penetration == events[4].penetration);
		CHECK(contacts[2].normal == Vector2F(1.0f, 0.0f));
		CHECK(contacts[2].penetration == 8.0f);
	}
}

TEST_CASE("moving the other participant can separate later pairs without losing this half")
{
	for (bool indexed : { false, true })
	{
		CAPTURE(indexed);
		std::vector<DispatchEvent> events;
		DispatchCollider a(1, 0.0f, &events);
		DispatchCollider b(2, 2.0f, &events);
		DispatchCollider c(3, 4.0f, &events);
		a.set_response([](DispatchCollider&, CollisionObject& other)
			{
				if (other.tag() == 2u)
				{
					static_cast<DispatchCollider&>(other).set_x(100.0f);
				}
			});
		CollisionObject* objects[] = { &a, &b, &c };
		labrador::BroadPhase broad_phase;
		std::vector<Contact> contacts;
		find_contacts(objects, contacts, indexed ? &broad_phase : nullptr);
		dispatch_contacts(contacts);
		const std::vector<DispatchPair> expected{ { 1, 2 }, { 2, 1 }, { 1, 3 }, { 3, 1 } };
		CHECK(dispatch_order(events) == expected);
		REQUIRE(events.size() == 4);
		CHECK(events[1].normal == Vector2F(-1.0f, 0.0f));
		CHECK(events[1].penetration == 8.0f);
		CHECK(contacts.size() == 3); // Snapshot membership is not dispatch history.
	}
}

TEST_CASE("retirement from either callback half suppresses only later pairs")
{
	for (bool indexed : { false, true })
	{
		for (int response = 0; response < 4; ++response)
		{
			CAPTURE(indexed);
			CAPTURE(response);
			std::vector<DispatchEvent> events;
			DispatchCollider a(1, 0.0f, &events);
			DispatchCollider b(2, 2.0f, &events);
			DispatchCollider c(3, 4.0f, &events);
			a.set_response([response](DispatchCollider& self, CollisionObject& other)
				{
					if (other.tag() == 2u && response < 2)
					{
						(response == 0 ? self : other).set_for_deletion(true);
					}
				});
			b.set_response([response](DispatchCollider& self, CollisionObject& other)
				{
					if (other.tag() == 1u && response >= 2)
					{
						(response == 2 ? other : self).set_for_deletion(true);
					}
				});
			CollisionObject* objects[] = { &a, &b, &c };
			labrador::BroadPhase broad_phase;
			std::vector<Contact> contacts;
			find_contacts(objects, contacts, indexed ? &broad_phase : nullptr);
			dispatch_contacts(contacts);
			const bool retired_a = response == 0 || response == 2;
			const std::vector<DispatchPair> expected = retired_a ?
				std::vector<DispatchPair>{ { 1, 2 }, { 2, 1 }, { 2, 3 }, { 3, 2 } } :
				std::vector<DispatchPair>{ { 1, 2 }, { 2, 1 }, { 1, 3 }, { 3, 1 } };
			CHECK(dispatch_order(events) == expected);
			CHECK(a.for_deletion() == retired_a);
			CHECK(b.for_deletion() == !retired_a);
		}
	}
}
