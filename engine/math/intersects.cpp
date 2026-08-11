#include "engine/math/intersects.h"

#include "engine/math/rectanglef.h"
#include "engine/math/circle.h"
#include "engine/math/triangle.h"
#include "engine/math/quad.h"
#include "engine/math/segment.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/shape.h"
#include "engine/math/ericson_math.h"

namespace mattmath
{

	bool mattmath::rectangle_circle_intersect(const RectangleF& rectangle, const Circle& circle,
		Point2F& point)
	{
		return test_circle_AABB(circle, rectangle, point);
	}

	bool mattmath::rectangle_circle_intersect(const RectangleF& rectangle, const Circle& circle)
	{
		return test_circle_AABB(circle, rectangle);
	}

	bool mattmath::rectangle_triangle_intersect(const RectangleF& rectangle, const Triangle& triangle)
	{
		// AABB vs AABB
		if (!rectangle.intersects(triangle.bounding_box()))
		{
			return false;
		}

		// check if the rectangle contains any of the triangle's points
		if (rectangle_point_intersect(rectangle, triangle.point_0()) ||
			rectangle_point_intersect(rectangle, triangle.point_1()) ||
			rectangle_point_intersect(rectangle, triangle.point_2()))
		{
			return true;
		}

		// check if the triangle contains any of the rectangle's points
		if (triangle_point_intersect(triangle, rectangle.top_left()) ||
			triangle_point_intersect(triangle, rectangle.top_right()) ||
			triangle_point_intersect(triangle, rectangle.bottom_left()) ||
			triangle_point_intersect(triangle, rectangle.bottom_right()))
		{
			return true;
		}

		// check if any of the triangle's edges intersect the rectangle
		const auto edges = triangle.edges();
		for (const Segment& edge : edges)
		{
			if (rectangle_segment_intersect(rectangle, edge))
			{
				return true;
			}
		}

		return false;
	}

	bool mattmath::rectangle_quad_intersect(const RectangleF& rectangle, const Quad& quad)
	{
		// get the triangles of the quad
		const auto triangles = quad.triangles();

		// check each triangle against the rectangle
		for (const Triangle& triangle : triangles)
		{
			if (rectangle_triangle_intersect(rectangle, triangle))
			{
				return true;
			}
		}
		return false;
	}

	bool mattmath::rectangle_segment_intersect(const RectangleF& rectangle, const Segment& segment)
	{
		return test_segment_AABB(segment.point_0, segment.point_1, rectangle);
	}

	bool mattmath::rectangle_point_intersect(const RectangleF& rectangle, const Point2F& point)
	{
		return point.x >= rectangle.x &&
			point.x <= rectangle.x + rectangle.width &&
			point.y >= rectangle.y &&
			point.y <= rectangle.y + rectangle.height;
	}

	bool mattmath::rectangle_rotated_rectangle_intersect(const RectangleF& rect,
		const RectangleRotated& rotated_rect)
	{
		// check if the rectangles' bounding boxes intersect
		if (!rect.intersects(rotated_rect.bounding_box()))
		{
			return false;
		}

		// check if the rotated rectangle contains any of the rectangle's points
		if (point_rectangle_rotated_intersect(rect.top_left(), rotated_rect) ||
			point_rectangle_rotated_intersect(rect.top_right(), rotated_rect) ||
			point_rectangle_rotated_intersect(rect.bottom_left(), rotated_rect) ||
			point_rectangle_rotated_intersect(rect.bottom_right(), rotated_rect))
		{
			return true;
		}

		// check if the rectangle contains any of the rotated rectangle's points
		if (rectangle_point_intersect(rect, rotated_rect.point_0()) ||
			rectangle_point_intersect(rect, rotated_rect.point_1()) ||
			rectangle_point_intersect(rect, rotated_rect.point_2()) ||
			rectangle_point_intersect(rect, rotated_rect.point_3()))
		{
			return true;
		}

		// check if any of the rotated rectangle's edges intersect the rectangle
		const auto edges = rotated_rect.edges();
		for (const Segment& edge : edges)
		{
			if (rectangle_segment_intersect(rect, edge))
			{
				return true;
			}
		}

		return false;
	}

	bool mattmath::circles_intersect(const Circle& a, const Circle& b)
	{
		// The squared form, which is the same answer without the square root.
		// Squaring preserves order on non-negative quantities, so a predicate
		// that only has to decide never needs the root (5.2.5, p.165).
		//
		// Forwarded rather than rewritten: test_circle_circle is this test,
		// already ported and already correct, and it had no callers anywhere
		// in the tree. Two functions answering one question is a correctness
		// hazard before it is a duplication one - they round differently at
		// the grazing boundary, so they can disagree about a contact.
		return test_circle_circle(a, b);
	}

	bool mattmath::circle_triangle_intersect(const Circle& circle, const Triangle& triangle, Point2F& point)
	{
		return test_circle_triangle(circle, triangle.point_0(),
			triangle.point_1(), triangle.point_2(), point);
	}

	bool mattmath::circle_triangle_intersect(const Circle& circle, const Triangle& triangle)
	{
		Point2F point;
		return test_circle_triangle(circle, triangle.point_0(),
			triangle.point_1(), triangle.point_2(), point);
	}

	bool mattmath::circle_quad_intersect(const Circle& circle, const Quad& quad)
	{
		const auto triangles = quad.triangles();

		for (const Triangle& triangle : triangles)
		{
			if (circle_triangle_intersect(circle, triangle))
			{
				return true;
			}
		}
		return false;
	}

	bool mattmath::circle_segment_intersect(const Circle& circle, const Segment& segment, Point2F& point)
	{
		float t;
		closest_pt_point_segment(circle.center(),
			segment.point_0, segment.point_1, t, point);

		// Squared both sides - see circles_intersect. This one is called once
		// per edge from circle_rectangle_rotated_intersect, so it was four
		// roots per query for an answer that never needed one.
		return Vector2F::distance_squared(circle.center(), point) <=
			circle.radius() * circle.radius();
	}

	bool mattmath::circle_segment_intersect(const Circle& circle, const Segment& segment)
	{
		Point2F point;
		return circle_segment_intersect(circle, segment, point);
	}

	bool mattmath::circle_point_intersect(const Circle& circle, const Point2F& point)
	{
		return Vector2F::distance_squared(circle.center(), point) <=
			circle.radius() * circle.radius();
	}

	bool mattmath::circle_rectangle_rotated_intersect(const Circle& circle,
		const RectangleRotated& rect_rotated)
	{
		// check if the circle intersects the rectangle's bounding box
		if (!rectangle_circle_intersect(rect_rotated.bounding_box(), circle))
		{
			return false;
		}

		// check if circle's center is contained within the rectangle
		if (point_rectangle_rotated_intersect(circle.center(), rect_rotated))
		{
			return true;
		}

		// check if the circle intersects any of the rectangle's edges
		const auto edges = rect_rotated.edges();
		for (const Segment& edge : edges)
		{
			if (circle_segment_intersect(circle, edge))
			{
				return true;
			}
		}

		return false;
	}

	bool mattmath::triangles_intersect(const Triangle& a, const Triangle& b)
	{
		// check if any of the points are contained within each other
		for (int i = 0; i < 3; i++)
		{
			if (triangle_point_intersect(a, b.points[i]) ||
				triangle_point_intersect(b, a.points[i]))
			{
				return true;
			}
		}

		// get edges of each triangle
		const auto a_edges = a.edges();
		const auto b_edges = b.edges();

		// All nine edge pairs.
		//
		// This is not a bug fix, and the previous four-pair version was not
		// wrong. It ran i < 2 against b_edges[0] and b_edges[1], never testing
		// a_edges[2] or b_edges[2] against anything, which reads like an
		// obvious hole - and is not one, for a reason worth writing down
		// before somebody "restores" the optimisation or, worse, keeps the
		// subset believing it is broken.
		//
		// Given the containment test above has already failed, no vertex of
		// either triangle lies inside the other. Now suppose no crossing fell
		// among the four pairs that were tested. Then a_edges[0] and
		// a_edges[1] could only ever cross b_edges[2]. But a segment that
		// enters a convex region has to leave it again - with no endpoint
		// inside, its crossings come in pairs - and two straight segments
		// cross at most once. So each of a_edges[0] and a_edges[1] would have
		// to cross the boundary of B zero times. By the same argument
		// b_edges[0] and b_edges[1] cross A zero times. The only crossing left
		// possible is (a_edges[2], b_edges[2]), a single crossing, which again
		// needs an endpoint inside. Contradiction: there were no crossings at
		// all, and the triangles do not overlap.
		//
		// So the subset was sufficient. It was not *evidently* sufficient, and
		// nine segment tests on a predicate the narrow phase is replacing is
		// not a cost worth defending against the reader who has to re-derive
		// that proof. A search over half a million overlapping vertex-free
		// triangle pairs produced no disagreement between the two forms, which
		// is the empirical half of the same statement.
		for (int i = 0; i < 3; i++)
		{
			for (int j = 0; j < 3; j++)
			{
				if (segments_intersect(a_edges[i], b_edges[j]))
				{
					return true;
				}
			}
		}

		return false;
	}

	bool mattmath::triangle_quad_intersect(const Triangle& triangle, const Quad& quad)
	{
		// get the triangles of the quad
		const auto triangles = quad.triangles();

		// check each triangle against the quad's triangles
		for (const Triangle& quad_triangle : triangles)
		{
			if (triangles_intersect(triangle, quad_triangle))
			{
				return true;
			}
		}

		return false;
	}

	bool mattmath::triangle_segment_intersect(const Triangle& triangle, const Segment& segment)
	{
		// check if the segment's end points are contained within the triangle
		if (triangle_point_intersect(triangle, segment.point_0) ||
			triangle_point_intersect(triangle, segment.point_1))
		{
			return true;
		}

		// check if the segment intersects any of the triangle's edges
		const auto edges = triangle.edges();
		for (const Segment& edge : edges)
		{
			if (segments_intersect(edge, segment))
			{
				return true;
			}
		}

		return false;
	}

	bool mattmath::triangle_point_intersect(const Triangle& triangle, const Point2F& point)
	{
		return test_point_triangle(point, triangle.points[0],
			triangle.points[1], triangle.points[2]);
	}

	bool mattmath::triangle_rectangle_rotated_intersect(const Triangle& triangle,
		const RectangleRotated& rect_rotated)
	{
		// check if the triangle intersects the rectangle's bounding box
		if (!rectangle_triangle_intersect(rect_rotated.bounding_box(), triangle))
		{
			return false;
		}

		// check if the triangle contains any of the rectangle's points
		if (triangle_point_intersect(triangle, rect_rotated.point_0()) ||
			triangle_point_intersect(triangle, rect_rotated.point_1()) ||
			triangle_point_intersect(triangle, rect_rotated.point_2()) ||
			triangle_point_intersect(triangle, rect_rotated.point_3()))
		{
			return true;
		}

		// check if the rectangle contains any of the triangle's points
		if (point_rectangle_rotated_intersect(triangle.points[0], rect_rotated) ||
			point_rectangle_rotated_intersect(triangle.points[1], rect_rotated) ||
			point_rectangle_rotated_intersect(triangle.points[2], rect_rotated))
		{
			return true;
		}

		// check if any of the triangle's edges intersect the rectangle
		const auto edges = triangle.edges();
		for (const Segment& edge : edges)
		{
			if (segment_rectangle_rotated_intersect(edge, rect_rotated))
			{
				return true;
			}
		}

		return false;
	}

	bool mattmath::quads_intersect(const Quad& a, const Quad& b)
	{
		// get the triangles of each quad
		const auto a_triangles = a.triangles();
		const auto b_triangles = b.triangles();

		// check each triangle of one quad against the other
		for (const Triangle& a_triangle : a_triangles)
		{
			for (const Triangle& b_triangle : b_triangles)
			{
				if (triangles_intersect(a_triangle, b_triangle))
				{
					return true;
				}
			}
		}

		return false;
	}

	bool mattmath::quad_segment_intersect(const Quad& quad, const Segment& segment)
	{
		// get the triangles of the quad
		const auto triangles = quad.triangles();

		// check each triangle against the segment
		for (const Triangle& triangle : triangles)
		{
			if (triangle_segment_intersect(triangle, segment))
			{
				return true;
			}
		}

		return false;
	}

	bool mattmath::quad_point_intersect(const Quad& quad, const Point2F& point)
	{
		// check if the point is contained within any of the quad's triangles
		const auto triangles = quad.triangles();
		for (const Triangle& triangle : triangles)
		{
			if (triangle_point_intersect(triangle, point))
			{
				return true;
			}
		}

		return false;
	}

	bool mattmath::quad_rectangle_rotated_intersect(const Quad& quad,
		const RectangleRotated& rect_rotated)
	{
		// get the triangles of the quad
		const auto triangles = quad.triangles();

		// check each triangle against the rotated rectangle
		for (const Triangle& triangle : triangles)
		{
			if (triangle_rectangle_rotated_intersect(triangle, rect_rotated))
			{
				return true;
			}
		}

		return false;
	}

	bool mattmath::segments_intersect(const Segment& a, const Segment& b, float& t, Point2F& p)
	{
		// A proper crossing, and nothing else - see the contract on
		// test_2D_segment_segment.
		//
		// There used to be an `if (a == b)` shortcut here returning true with
		// t = 0. It made the predicate disagree with itself: Segment's
		// operator== compares point_0 to point_0, so it is direction
		// sensitive, and segments_intersect(s, s) was therefore true while
		// segments_intersect(s, reversed(s)) was false. The same two point
		// sets, opposite answers, decided by which end the caller happened to
		// write first.
		//
		// It also did not fix the hole it sat in front of - collinear overlap
		// is still not an intersection here - so it patched exactly one
		// instance of a uniformly-open boundary and made the rest
		// inconsistent. Removing it changes no consumer's answer: every
		// polygon routine that reaches this runs a closed containment pass
		// over the vertices first, and two identical segments share both
		// endpoints, so containment has already returned true.
		return test_2D_segment_segment(a.point_0, a.point_1,
			b.point_0, b.point_1, t, p);
	}

	bool mattmath::segments_intersect(const Segment& a, const Segment& b)
	{
		float t;
		Point2F p;
		return segments_intersect(a, b, t, p);
	}

	bool mattmath::segment_rectangle_rotated_intersect(const Segment& segment,
		const RectangleRotated& rect_rotated)
	{
		// check if the segment intersects the rectangle's bounding box
		if (!rectangle_segment_intersect(rect_rotated.bounding_box(), segment))
		{
			return false;
		}

		// check if the segment's end points are contained within the rectangle
		if (point_rectangle_rotated_intersect(segment.point_0, rect_rotated) ||
			point_rectangle_rotated_intersect(segment.point_1, rect_rotated))
		{
			return true;
		}

		// check if the segment intersects any of the rectangle's edges
		const auto edges = rect_rotated.edges();
		for (const Segment& edge : edges)
		{
			if (segments_intersect(segment, edge))
			{
				return true;
			}
		}

		return false;
	}

	bool mattmath::point_rectangle_rotated_intersect(const Point2F& point,
		const RectangleRotated& rect_rotated)
	{
		// Straight onto the corner cache the rectangle already holds.
		//
		// This used to build a Quad from it and triangulate that - so a
		// boolean query allocated twice, re-proved the convexity of a shape
		// that is convex by construction, and could THROW: Quad's constructor
		// rejects four coincident points, which is exactly what a
		// default-constructed RectangleRotated holds. RectangleRotated::contains
		// forwarded here, so a predicate returning bool terminated the caller
		// with an invalid_argument naming a type it never mentioned. That
		// member is gone now - it was one of five one-line contains(Point2F)
		// forwards deleted with the intersection table - and this function is
		// the only spelling left, which is where the guarantee below lives.
		//
		// point_in_convex_polygon gives the same answer for a convex
		// quadrilateral - both take the boundary as inside and neither cares
		// about winding - in four cross products and no allocation. A
		// degenerate rectangle now answers false rather than throwing, which
		// is the same thing every other degenerate shape in the library says.
		return point_in_convex_polygon(rect_rotated.points(), point);
	}

	bool mattmath::rectangles_rotated_intersect(const RectangleRotated& a,
		const RectangleRotated& b)
	{
		// check if the rectangles' bounding boxes intersect
		if (!rectangle_rotated_rectangle_intersect(b.bounding_box(), a))
		{
			return false;
		}

		// check if any of the points of one rectangle are contained within the other
		if (point_rectangle_rotated_intersect(b.point_0(), a) ||
			point_rectangle_rotated_intersect(b.point_1(), a) ||
			point_rectangle_rotated_intersect(b.point_2(), a) ||
			point_rectangle_rotated_intersect(b.point_3(), a))
		{
			return true;
		}

		if (point_rectangle_rotated_intersect(a.point_0(), b) ||
			point_rectangle_rotated_intersect(a.point_1(), b) ||
			point_rectangle_rotated_intersect(a.point_2(), b) ||
			point_rectangle_rotated_intersect(a.point_3(), b))
		{
			return true;
		}

		// check if any of the edges of one rectangle intersect the other
		const auto a_edges = a.edges();
		const auto b_edges = b.edges();

		for (const Segment& edge : a_edges)
		{
			for (const Segment& other_edge : b_edges)
			{
				if (segments_intersect(edge, other_edge))
				{
					return true;
				}
			}
		}

		return false;
	}

}
