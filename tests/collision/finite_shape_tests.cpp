#include <doctest/doctest.h>

#include "engine/collision/broad_phase.h"
#include "engine/collision/contacts.h"
#include "engine/collision/narrow_phase.h"
#include "engine/math/circle.h"
#include "engine/math/quad.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/rectanglef.h"
#include "engine/math/triangle.h"
#include "tests/collision/stub_collider.h"

#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace labrador;

namespace
{
	class BoxShape final : public Shape
	{
	public:
		RectangleF bounding_box() const override { return RectangleF(0, 0, 10, 10); }
		ShapeType shape_type() const override { return this->kind; }
		void offset(const Vector2F&) override {}
		Point2F center() const override { return Vector2F(5, 5); }
		void inflate(float) override {}

		ShapeType kind = ShapeType::rectangle;
	};

	class ShapeCollider final : public StubCollider
	{
	public:
		explicit ShapeCollider(const Shape* shape) :
			StubCollider(RectangleF(), 1, 1, 73), shape_(shape) {}

		const Shape* shape() const override { return this->shape_; }
		void on_contact(const CollisionObject&, const Vector2F&, float) override
		{
			++this->received;
			if (this->response) this->response();
		}

		int received = 0;
		std::function<void()> response;

	private:
		const Shape* shape_ = nullptr;
	};
}

TEST_CASE("rectangle-discriminated custom shapes need no concrete rectangle cast")
{
	BoxShape shape;
	ShapeCollider custom(&shape);
	const RectangleF rectangle(2, 2, 10, 10);
	ShapeCollider other(&rectangle);
	CHECK_NOTHROW(custom.validate_finite_shape());
	CollisionObject* objects[] = { &custom, &other };
	std::vector<Contact> contacts;
	find_contacts(objects, contacts);
	REQUIRE(contacts.size() == 1);
	shape.kind = ShapeType::none;
	CHECK_THROWS_WITH_AS(custom.validate_finite_shape(),
		"CollisionObject::validate_finite_shape: tag 73 shape has no supported type",
		std::invalid_argument);
}

TEST_CASE("collider validation names each nonfinite rectangle field without mutating it")
{
	const std::array<float RectangleF::*, 4> fields = {
		&RectangleF::x, &RectangleF::y, &RectangleF::width, &RectangleF::height };
	const std::array<const char*, 4> messages = {
		"CollisionObject::validate_finite_shape: tag 73 rectangle.x must be finite",
		"CollisionObject::validate_finite_shape: tag 73 rectangle.y must be finite",
		"CollisionObject::validate_finite_shape: tag 73 rectangle.width must be finite",
		"CollisionObject::validate_finite_shape: tag 73 rectangle.height must be finite" };
	for (size_t index = 0; index < fields.size(); ++index)
	{
		for (const float poison : { std::numeric_limits<float>::quiet_NaN(),
			std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() })
		{
			CAPTURE(index);
			CAPTURE(poison);
			RectangleF rectangle(0, 0, 10, 10);
			rectangle.*fields[index] = poison;
			ShapeCollider object(&rectangle);
			CHECK_THROWS_WITH_AS(object.validate_finite_shape(), messages[index],
				std::invalid_argument);
			CHECK_FALSE(object.for_deletion());
			CHECK_FALSE(std::isfinite(rectangle.*fields[index]));
		}
	}
}

TEST_CASE("collider validation checks defining fields rather than a reduced polygon box")
{
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float infinity = std::numeric_limits<float>::infinity();
	Triangle triangle(Vector2F(0, 0), Vector2F(nan, 5), Vector2F(10, 10));
	REQUIRE(std::isfinite(triangle.bounding_box().x));
	ShapeCollider triangle_object(&triangle);
	CHECK_THROWS_WITH_AS(triangle_object.validate_finite_shape(),
		"CollisionObject::validate_finite_shape: tag 73 triangle.point_1 must be finite",
		std::invalid_argument);
	const RectangleF good(0, 0, 10, 10);
	CHECK_FALSE(narrow_phase(triangle, good).has_value());

	Circle circle(Vector2F(0, 0), infinity);
	ShapeCollider circle_object(&circle);
	CHECK_THROWS_WITH_AS(circle_object.validate_finite_shape(),
		"CollisionObject::validate_finite_shape: tag 73 circle.radius must be finite",
		std::invalid_argument);
	circle = Circle(Vector2F(nan, 0), 2);
	CHECK_THROWS_AS(circle_object.validate_finite_shape(), std::invalid_argument);

	Quad quad(RectangleF(0, 0, 10, 10));
	quad.offset(Vector2F(infinity, 0));
	ShapeCollider quad_object(&quad);
	CHECK_THROWS_AS(quad_object.validate_finite_shape(), std::invalid_argument);

	RectangleRotated rotated(Vector2F(0, 0), Vector2F(1, 0), Vector2F(0, 1), Vector2F(2, 2));
	ShapeCollider rotated_object(&rotated);
	rotated.set_center(Vector2F(0, nan));
	CHECK_THROWS_AS(rotated_object.validate_finite_shape(), std::invalid_argument);
	rotated.set_center(Vector2F(0, 0));
	rotated.set_half_extents(Vector2F(infinity, 2));
	CHECK_THROWS_AS(rotated_object.validate_finite_shape(), std::invalid_argument);

	ShapeCollider missing(nullptr);
	CHECK_THROWS_WITH_AS(missing.validate_finite_shape(),
		"CollisionObject::validate_finite_shape: tag 73 shape() is null", std::invalid_argument);
}

TEST_CASE("finite degenerate and enormous defining fields retain their existing semantics")
{
	RectangleF rectangle(0, 0, -10, 0);
	Triangle triangle(Vector2F(0, 0), Vector2F(1, 1), Vector2F(2, 2));
	Circle circle(Vector2F(0, 0), 2.0e38f);
	REQUIRE_FALSE(std::isfinite(circle.bounding_box().width));
	for (const Shape* shape : { static_cast<const Shape*>(&rectangle),
		static_cast<const Shape*>(&triangle), static_cast<const Shape*>(&circle) })
	{
		ShapeCollider object(shape);
		CHECK_NOTHROW(object.validate_finite_shape());
	}
	const RectangleF good(0, 0, 10, 10);
	CHECK_FALSE(narrow_phase(triangle, good).has_value());
	CHECK(narrow_phase(circle, good).has_value());
}

TEST_CASE("find_contacts refuses all active invalid inputs before emitting any pair")
{
	RectangleF a(0, 0, 10, 10);
	RectangleF b(2, 2, 10, 10);
	RectangleF invalid(std::numeric_limits<float>::quiet_NaN(), 0, 1, 1);
	ShapeCollider first(&a);
	ShapeCollider second(&b);
	ShapeCollider last(&invalid);
	std::vector<CollisionObject*> objects{ &first, &second, &last };
	std::vector<Contact> contacts{ Contact{} };
	BroadPhase broad_phase;
	SUBCASE("all pairs")
	{
		CHECK_THROWS_AS(find_contacts(objects, contacts), std::invalid_argument);
	}
	SUBCASE("indexed pairs")
	{
		CHECK_THROWS_AS(find_contacts(objects, contacts, &broad_phase), std::invalid_argument);
	}
	SUBCASE("a lone collider is still checked")
	{
		objects = { &last };
		CHECK_THROWS_AS(find_contacts(objects, contacts), std::invalid_argument);
	}
	SUBCASE("a zero mask does not hide invalid geometry")
	{
		StubCollider masked(invalid, 1, 0);
		CollisionObject* masked_objects[] = { &masked };
		CHECK_THROWS_AS(find_contacts(masked_objects, contacts), std::invalid_argument);
	}
	CHECK(contacts.empty());
	CHECK(first.received == 0);
	CHECK(second.received == 0);
	CHECK_FALSE(last.for_deletion());
	last.set_for_deletion(true);
	CHECK_NOTHROW(find_contacts(objects, contacts, &broad_phase));
}

TEST_CASE("dispatch reports geometry corrupted by an earlier callback without rollback")
{
	RectangleF a(0, 0, 10, 10);
	RectangleF b(2, 2, 10, 10);
	RectangleF c(4, 4, 10, 10);
	ShapeCollider first(&a);
	ShapeCollider second(&b);
	ShapeCollider last(&c);
	CollisionObject* objects[] = { &first, &second, &last };
	std::vector<Contact> contacts;
	find_contacts(objects, contacts);
	REQUIRE(contacts.size() == 3);
	first.response = [&c]() { c.x = std::numeric_limits<float>::quiet_NaN(); };
	CHECK_THROWS_AS(dispatch_contacts(contacts), std::invalid_argument);
	CHECK(first.received == 1);
	CHECK(second.received == 1);
	CHECK(last.received == 0);
	CHECK(std::isnan(c.x));
}
