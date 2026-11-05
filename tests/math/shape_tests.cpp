#include <doctest/doctest.h>

#include "engine/math/quad.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/rectanglef.h"
#include "engine/math/triangle.h"
#include "engine/math/vector2f.h"

#include <array>

using namespace labrador;

// The subject is the contracts every Shape implementation states, rather
// than the base class: edges() was deliberately taken off Shape (see the
// NOT HERE note in shape.h), so the ordering below is a promise each
// concrete shape keeps on its own account.
TEST_SUITE("Shape")
{
	TEST_CASE("edges are fixed-size and in the order the accessors name")
	{
		// edges() is a std::array per shape, not a pure virtual on Shape
		// returning std::vector<Segment> - which is a heap allocation, on the
		// narrow phase's own path, for a list whose length is known at compile
		// time. These checks are the contract: each slot is the matching named
		// accessor.
		//
		// The order is worth a test rather than a comment. A caller that
		// stores a rectangle's faces by edge index and recovers them by
		// position puts three of four faces on the wrong side if it
		// assumes {top, right, bottom, left} rather than the order below.
		const RectangleF rect(0.0f, 0.0f, 10.0f, 20.0f);
		const std::array<Segment, 4> rect_edges = rect.edges();
		static_assert(rect_edges.size() == 4);
		CHECK(rect_edges[0] == rect.top_edge());
		CHECK(rect_edges[1] == rect.bottom_edge());
		CHECK(rect_edges[2] == rect.left_edge());
		CHECK(rect_edges[3] == rect.right_edge());

		const Triangle tri(Vector2F(0.0f, 0.0f), Vector2F(10.0f, 0.0f),
			Vector2F(0.0f, 10.0f));
		const std::array<Segment, 3> tri_edges = tri.edges();
		static_assert(tri_edges.size() == 3);
		CHECK(tri_edges[0] == tri.edge_0());
		CHECK(tri_edges[1] == tri.edge_1());
		CHECK(tri_edges[2] == tri.edge_2());

		const Quad quad(rect);
		const std::array<Segment, 4> quad_edges = quad.edges();
		static_assert(quad_edges.size() == 4);
		CHECK(quad_edges[0] == quad.edge_0());
		CHECK(quad_edges[1] == quad.edge_1());
		CHECK(quad_edges[2] == quad.edge_2());
		CHECK(quad_edges[3] == quad.edge_3());

		const RectangleRotated rr(Point2F::zero, Vector2F::direction_right,
			Vector2F::direction_down, Vector2F(5.0f, 5.0f));
		const std::array<Segment, 4> rr_edges = rr.edges();
		static_assert(rr_edges.size() == 4);
		CHECK(rr_edges[0] == rr.edge_0());
		CHECK(rr_edges[1] == rr.edge_1());
		CHECK(rr_edges[2] == rr.edge_2());
		CHECK(rr_edges[3] == rr.edge_3());
	}

	TEST_CASE("two shapes' bounding boxes intersect when they overlap or touch")
	{
		const RectangleF box(0.0f, 0.0f, 10.0f, 10.0f);
		const Triangle beside(Point2F(10.0f, 0.0f), Point2F(20.0f, 0.0f),
			Point2F(10.0f, 10.0f));
		const Triangle away(Point2F(30.0f, 0.0f), Point2F(40.0f, 0.0f),
			Point2F(30.0f, 10.0f));
		const Quad over(Point2F(5.0f, 5.0f), Point2F(15.0f, 5.0f),
			Point2F(15.0f, 15.0f), Point2F(5.0f, 15.0f));

		const Shape& shape = box;
		CHECK(shape.bounding_boxes_intersect(over));
		CHECK(over.bounding_boxes_intersect(shape));
		CHECK(shape.bounding_boxes_intersect(beside) ==
			box.intersects(beside.bounding_box()));
		CHECK_FALSE(shape.bounding_boxes_intersect(away));
		CHECK_FALSE(away.bounding_boxes_intersect(shape));
	}
}
