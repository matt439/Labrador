#pragma once

#include "engine/math/vector2f.h"

// The pairwise intersection vocabulary: every shape in this module against
// every other, as free functions.
//
// WHY IT IS HERE AT ALL, SETTLED ONCE. This table was measured as having no
// caller outside this file and the tests - no engine, bench, sample or client
// use - and put forward for deletion on that evidence, which would have taken
// circle.{h,cpp} and three Ericson routines out with it, some 1,400 lines.
// It is kept, and NOT because the count has since changed. The count is not
// the question.
//
// The question is what a game does when it needs one of these. A 2D engine
// whose client has to ask whether a segment crosses a rotated rectangle, and
// write the answer itself, has pushed geometry across the boundary
// (PHILOSOPHY, The boundary) - the game would be supplying mechanism, and a
// second definition of engine arithmetic would live outside the engine's
// tests, where the degenerate cases both review rounds worked on are not
// checked. That has already happened once, to a vector rotation deleted the
// same way and reinstated in vector2f.h. Every one of these is a predicate a
// 2D game can reasonably want, so every one of them belongs here, at whatever
// call count.
//
// The measurement itself did not survive either, which is worth recording
// because it is the reason to distrust the next one: ColourWars now calls
// rectangle_point_intersect, having moved onto it when a member it used was
// deleted. Zero callers became one without a line of this file changing.
//
// The collision module's not adopting the table is a real observation and a
// separate question. narrow_phase builds its own Polygon and runs its own
// separating-axis test; whether it should instead be built on these is a
// design question about the collision module, not evidence against this one.

namespace mattmath
{
	struct RectangleF;
	struct Circle;
	struct Triangle;
	struct Quad;
	struct Segment;
	struct RectangleRotated;

	// No rectangles_intersect here: RectangleF::intersects(const RectangleF&)
	// is that predicate, and is the one member of the old table with
	// production callers.

	bool rectangle_circle_intersect(const mattmath::RectangleF& rectangle,
		const mattmath::Circle& circle, mattmath::Point2F& point);

	bool rectangle_circle_intersect(const mattmath::RectangleF& rectangle,
		const mattmath::Circle& circle);

	bool rectangle_triangle_intersect(const mattmath::RectangleF& rectangle, 
		const mattmath::Triangle& triangle);

	bool rectangle_quad_intersect(const mattmath::RectangleF& rectangle,
		const mattmath::Quad& quad);

	bool rectangle_segment_intersect(const mattmath::RectangleF& rectangle, 
		const mattmath::Segment& segment);

	bool rectangle_point_intersect(const mattmath::RectangleF& rectangle, 
		const mattmath::Point2F& point);

	bool rectangle_rotated_rectangle_intersect(const mattmath::RectangleF& rect,
		const mattmath::RectangleRotated& rotated_rect);

	bool circles_intersect(const mattmath::Circle& a,
		const mattmath::Circle& b);

	bool circle_triangle_intersect(const mattmath::Circle& circle,
		const mattmath::Triangle& triangle, mattmath::Point2F& point);

	bool circle_triangle_intersect(const mattmath::Circle& circle,
		const mattmath::Triangle& triangle);

	bool circle_quad_intersect(const mattmath::Circle& circle,
		const mattmath::Quad& quad);

	bool circle_segment_intersect(const mattmath::Circle& circle,
		const mattmath::Segment& segment, mattmath::Point2F& point);

	bool circle_segment_intersect(const mattmath::Circle& circle,
		const mattmath::Segment& segment);

	bool circle_point_intersect(const mattmath::Circle& circle, 
		const mattmath::Point2F& point);

	bool circle_rectangle_rotated_intersect(const mattmath::Circle& circle,
		const mattmath::RectangleRotated& rect_rotated);

	bool triangles_intersect(const mattmath::Triangle& a, 
		const mattmath::Triangle& b);

	bool triangle_quad_intersect(const mattmath::Triangle& triangle, 
		const mattmath::Quad& quad);

	bool triangle_segment_intersect(const mattmath::Triangle& triangle, 
		const mattmath::Segment& segment);

	bool triangle_point_intersect(const mattmath::Triangle& triangle,
		const mattmath::Point2F& point);

	bool triangle_rectangle_rotated_intersect(const mattmath::Triangle& triangle,
		const mattmath::RectangleRotated& rect_rotated);

	bool quads_intersect(const mattmath::Quad& a, const mattmath::Quad& b);

	bool quad_segment_intersect(const mattmath::Quad& quad,
		const mattmath::Segment& segment);

	bool quad_point_intersect(const mattmath::Quad& quad, 
		const mattmath::Point2F& point);

	bool quad_rectangle_rotated_intersect(const mattmath::Quad& quad, 
		const mattmath::RectangleRotated& rect_rotated);

	bool segments_intersect(const mattmath::Segment& a, const mattmath::Segment& b,
		float& t, mattmath::Point2F& p);

	bool segments_intersect(const mattmath::Segment& a, const mattmath::Segment& b);

	bool segment_rectangle_rotated_intersect(const mattmath::Segment& segment,
		const mattmath::RectangleRotated& rect_rotated);

	bool point_rectangle_rotated_intersect(const mattmath::Point2F& point,
		const mattmath::RectangleRotated& rect_rotated);

	bool rectangles_rotated_intersect(const mattmath::RectangleRotated& a,
		const mattmath::RectangleRotated& b);
}
