#include <doctest/doctest.h>

#include "engine/math/quad.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/rectanglef.h"
#include "engine/math/triangle.h"
#include "engine/math/vector2f.h"

#include <array>

using namespace mattmath;

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
		// The order is worth a test rather than a comment. Encoding a
		// rectangle's paintable faces as edges and recovering them by
		// positional index put three of four faces on the wrong side of
		// every paintable structure in the game, because the decoder
		// assumed {top, right, bottom, left}.
		const RectangleF rect(0.0f, 0.0f, 10.0f, 20.0f);
		const auto rect_edges = rect.edges();
		static_assert(rect_edges.size() == 4);
		CHECK(rect_edges[0] == rect.top_edge());
		CHECK(rect_edges[1] == rect.bottom_edge());
		CHECK(rect_edges[2] == rect.left_edge());
		CHECK(rect_edges[3] == rect.right_edge());

		const Triangle tri(Vector2F(0.0f, 0.0f), Vector2F(10.0f, 0.0f),
			Vector2F(0.0f, 10.0f));
		const auto tri_edges = tri.edges();
		static_assert(tri_edges.size() == 3);
		CHECK(tri_edges[0] == tri.edge_0());
		CHECK(tri_edges[1] == tri.edge_1());
		CHECK(tri_edges[2] == tri.edge_2());

		const Quad quad(rect);
		const auto quad_edges = quad.edges();
		static_assert(quad_edges.size() == 4);
		CHECK(quad_edges[0] == quad.edge_0());
		CHECK(quad_edges[3] == quad.edge_3());

		const RectangleRotated rr(Point2F::ZERO, Vector2F::DIRECTION_RIGHT,
			Vector2F::DIRECTION_DOWN, Vector2F(5.0f, 5.0f));
		const auto rr_edges = rr.edges();
		static_assert(rr_edges.size() == 4);
		CHECK(rr_edges[0] == rr.edge_0());
		CHECK(rr_edges[3] == rr.edge_3());
	}
}
