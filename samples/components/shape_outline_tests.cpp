#include <doctest/doctest.h>

#include "samples/components/shape_outline.h"

#include "engine/math/circle.h"
#include "engine/math/quad.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/triangle.h"

#include <array>
#include <limits>
#include <stdexcept>

using namespace labrador;
using components::outline_edges;

namespace
{
	class TaggedShape : public Shape
	{
	public:
		explicit TaggedShape(ShapeType type) : type_(type) {}
		RectangleF bounding_box() const override { return {}; }
		ShapeType shape_type() const override { return this->type_; }
		void offset(const Vector2F&) override {}
		Vector2F center() const override { return {}; }
		void inflate(float) override {}
	private:
		ShapeType type_;
	};
}

TEST_SUITE("copyable shape outline geometry")
{
	TEST_CASE("rectangle triangle quad and rotated rectangle retain their boundary order")
	{
		std::array<Segment, 4> edges;
		const RectangleF rectangle(10, 20, 30, 40);
		REQUIRE(outline_edges(rectangle, edges, 0) == 4);
		CHECK(edges[0] == Segment(10, 20, 40, 20));
		CHECK(edges[1] == Segment(40, 20, 40, 60));
		CHECK(edges[2] == Segment(40, 60, 10, 60));
		CHECK(edges[3] == Segment(10, 60, 10, 20));

		const Triangle triangle(10, 10, 30, 10, 20, 40);
		REQUIRE(outline_edges(triangle, edges, 0) == 3);
		CHECK(edges[0] == Segment(10, 10, 30, 10));
		CHECK(edges[1] == Segment(30, 10, 20, 40));
		CHECK(edges[2] == Segment(20, 40, 10, 10));
		CHECK(outline_edges(TriangleRightAxisAligned(0, 0, 10, 0, 0, 10), edges, 0) == 3);

		const Quad quad(Vector2F(1, 2), Vector2F(12, 3),
			Vector2F(11, 15), Vector2F(0, 12));
		REQUIRE(outline_edges(quad, edges, 0) == 4);
		for (std::size_t index = 0; index < edges.size(); ++index)
		{
			CHECK(edges[index].point_0 == quad.points()[index]);
			CHECK(edges[index].point_1 == quad.points()[(index + 1) % 4]);
		}

		const RectangleRotated rotated(Vector2F(10, 20), Vector2F(0, 1),
			Vector2F(-1, 0), Vector2F(3, 2));
		REQUIRE(outline_edges(rotated, edges, 0) == 4);
		for (std::size_t index = 0; index < edges.size(); ++index)
		{
			CHECK(edges[index].point_0 == rotated.points()[index]);
			CHECK(edges[index].point_1 == rotated.points()[(index + 1) % 4]);
		}
	}

	TEST_CASE("circle chords use the explicit count and close exactly")
	{
		std::array<Segment, components::max_outline_segments> edges;
		const Circle circle(50, 50, 10);
		for (const int count : { 3, 4, 7, 32, components::max_outline_segments })
		{
			REQUIRE(outline_edges(circle, edges, count) == static_cast<std::size_t>(count));
			CHECK(edges[0].point_0 == Vector2F(60, 50));
			CHECK(edges[static_cast<std::size_t>(count - 1)].point_1 == edges[0].point_0);
			for (int index = 0; index < count; ++index)
			{
				const Segment& edge = edges[static_cast<std::size_t>(index)];
				CHECK((edge.point_0 - circle.center()).length() == doctest::Approx(10.0f));
			}
		}
		CHECK(outline_edges(Circle(1, 2, 0), std::span<Segment>(), 3) == 0);
	}

	TEST_CASE("finite degenerate polygons keep their edges without a fabricated interior")
	{
		std::array<Segment, 4> edges;
		const Triangle line(0, 0, 10, 0, 20, 0);
		REQUIRE(outline_edges(line, edges, 0) == 3);
		CHECK(edges[2] == Segment(20, 0, 0, 0));
		CHECK(outline_edges(RectangleRotated(), edges, 0) == 4);
		CHECK(outline_edges(Quad(), edges, 0) == 4);
		CHECK(outline_edges(RectangleF(10, 20, -3, -4), edges, 0) == 4);
		CHECK(edges[0] == Segment(10, 20, 7, 20));
	}

	TEST_CASE("bad geometry tags counts and capacity fail without changing output")
	{
		const Segment marker(-1, -2, -3, -4);
		std::array<Segment, 4> edges = { marker, marker, marker, marker };
		Triangle bad(0, 0, 10, 0, 10, 10);
		bad.points[2].y = std::numeric_limits<float>::quiet_NaN();
		CHECK_THROWS_AS(outline_edges(bad, edges, 0), std::invalid_argument);
		CHECK_THROWS_AS(outline_edges(TaggedShape(ShapeType::none), edges, 0), std::invalid_argument);
		CHECK_THROWS_AS(outline_edges(TaggedShape(ShapeType::rectangle), edges, 0), std::invalid_argument);
		CHECK_THROWS_AS(outline_edges(RectangleF(0, 0, 1, 1),
			std::span<Segment>(edges.data(), 3), 0), std::length_error);
		for (const int count : { -1, 0, 1, 2, components::max_outline_segments + 1 })
		{
			CHECK_THROWS_AS(outline_edges(Circle(0, 0, 1), edges, count), std::invalid_argument);
		}
		CHECK_THROWS_AS(outline_edges(Circle(0, 0, -1), edges, 4), std::invalid_argument);
		CHECK_THROWS_AS(outline_edges(Circle(std::numeric_limits<float>::infinity(), 0, 1),
			edges, 4), std::invalid_argument);
		volatile float runtime_largest = std::numeric_limits<float>::max();
		const float largest = runtime_largest;
		CHECK_THROWS_AS(outline_edges(Circle(largest, 0, largest), edges, 4), std::invalid_argument);
		CHECK_THROWS_AS(outline_edges(RectangleF(largest, 0, largest, 1), edges, 0),
			std::invalid_argument);
		for (const Segment& edge : edges) { CHECK(edge == marker); }
	}
}
