#pragma once

#include "engine/math/vector2f.h"

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
