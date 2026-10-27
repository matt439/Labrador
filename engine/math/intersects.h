#pragma once

#include "engine/math/vector2f.h"

// The pairwise intersection vocabulary: every shape in this module against
// every other, as free functions.
//
// EVERY ONE OF THESE BELONGS HERE, AT WHATEVER CALL COUNT. The count of
// callers is not the question; what a game does when it needs one of these
// is. A 2D engine whose client has to ask whether a segment crosses a rotated
// rectangle, and write the answer itself, has pushed geometry across the
// boundary (PHILOSOPHY, The boundary) - the game would be supplying
// mechanism, and a second definition of engine arithmetic would live outside
// the engine's tests, where the degenerate cases are not checked. Every one
// of these is a predicate a 2D game can reasonably want.
//
// The collision module does not call these. narrow_phase measures a pair -
// an axis and a depth - rather than answering yes or no, and builds its own
// polygons and runs its own separating-axis test to do it.

namespace labrador
{
	struct RectangleF;
	struct Circle;
	struct Triangle;
	struct Quad;
	struct Segment;
	struct RectangleRotated;

	// No rectangles_intersect here: RectangleF::intersects(const RectangleF&)
	// is that predicate.

	bool rectangle_circle_intersect(const RectangleF& rectangle,
		const Circle& circle, Point2F& point);

	bool rectangle_circle_intersect(const RectangleF& rectangle,
		const Circle& circle);

	bool rectangle_triangle_intersect(const RectangleF& rectangle, 
		const Triangle& triangle);

	bool rectangle_quad_intersect(const RectangleF& rectangle,
		const Quad& quad);

	bool rectangle_segment_intersect(const RectangleF& rectangle, 
		const Segment& segment);

	bool rectangle_point_intersect(const RectangleF& rectangle, 
		const Point2F& point);

	bool rectangle_rotated_rectangle_intersect(const RectangleF& rect,
		const RectangleRotated& rotated_rect);

	bool circles_intersect(const Circle& a,
		const Circle& b);

	bool circle_triangle_intersect(const Circle& circle,
		const Triangle& triangle, Point2F& point);

	bool circle_triangle_intersect(const Circle& circle,
		const Triangle& triangle);

	bool circle_quad_intersect(const Circle& circle,
		const Quad& quad);

	bool circle_segment_intersect(const Circle& circle,
		const Segment& segment, Point2F& point);

	bool circle_segment_intersect(const Circle& circle,
		const Segment& segment);

	bool circle_point_intersect(const Circle& circle, 
		const Point2F& point);

	bool circle_rectangle_rotated_intersect(const Circle& circle,
		const RectangleRotated& rect_rotated);

	bool triangles_intersect(const Triangle& a, 
		const Triangle& b);

	bool triangle_quad_intersect(const Triangle& triangle, 
		const Quad& quad);

	bool triangle_segment_intersect(const Triangle& triangle, 
		const Segment& segment);

	bool triangle_point_intersect(const Triangle& triangle,
		const Point2F& point);

	bool triangle_rectangle_rotated_intersect(const Triangle& triangle,
		const RectangleRotated& rect_rotated);

	bool quads_intersect(const Quad& a, const Quad& b);

	bool quad_segment_intersect(const Quad& quad,
		const Segment& segment);

	bool quad_point_intersect(const Quad& quad, 
		const Point2F& point);

	bool quad_rectangle_rotated_intersect(const Quad& quad, 
		const RectangleRotated& rect_rotated);

	bool segments_intersect(const Segment& a, const Segment& b,
		float& t, Point2F& p);

	bool segments_intersect(const Segment& a, const Segment& b);

	bool segment_rectangle_rotated_intersect(const Segment& segment,
		const RectangleRotated& rect_rotated);

	bool point_rectangle_rotated_intersect(const Point2F& point,
		const RectangleRotated& rect_rotated);

	bool rectangles_rotated_intersect(const RectangleRotated& a,
		const RectangleRotated& b);
}
