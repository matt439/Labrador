#pragma once

#include "engine/math/shape.h"
#include "engine/math/vector2f.h"
#include "engine/math/segment.h"

#include <array>

namespace mattmath
{
	struct Quad;

	struct RectangleRotated : public Shape
	{
		RectangleRotated() = default;
		RectangleRotated(const RectangleRotated&) = default;
		RectangleRotated(const mattmath::Point2F& center,
			const mattmath::Vector2F& x_axis, const mattmath::Vector2F& y_axis,
			const mattmath::Vector2F& hw_extents);
		RectangleRotated(const mattmath::Segment& center_line, float thickness);

		RectangleF bounding_box() const override;
		ShapeType shape_type() const override;
		void offset(const Vector2F& amount) override;
		Point2F center() const override;
		std::array<Segment, 4> edges() const;
		void inflate(float amount) override;

		Point2F x_axis() const;
		Point2F y_axis() const;
		Point2F axis(int axis) const;
		Point2F half_extents() const;
		float half_x_width() const;
		float half_y_width() const;
		float half_width(int axis) const;

		void set_center(const Point2F& center);

		// Rotating a rectangle means writing BOTH axes, and this is the only
		// way to do it.
		//
		// set_x_axis and set_y_axis each validate the candidate against the
		// partner axis as it currently stands, which is correct and which
		// makes them useless for turning anything: the new x axis has to be
		// perpendicular to the OLD y axis, so every genuine rotation is
		// rejected whichever of the two is called first. They remain for the
		// one thing they can express - reversing an axis - and this exists for
		// the thing they cannot.
		//
		// Throws std::invalid_argument unless the pair is orthonormal, and
		// commits nothing when it does.
		void set_axes(const Point2F& x_axis, const Point2F& y_axis);
		void set_x_axis(const Point2F& x_axis);
		void set_y_axis(const Point2F& y_axis);
		void set_half_extents(const Point2F& hw_extents);
		void set_half_x_width(float half_x_width);
		void set_half_y_width(float half_y_width);

		Point2F point_0() const;
		Point2F point_1() const;
		Point2F point_2() const;
		Point2F point_3() const;
		// The four corners, in the order point_0..point_3 name them. A
		// reference to storage the rectangle already holds, recomputed only
		// when something that defines it changes.
		const std::array<Point2F, 4>& points() const;

		Segment edge_0() const;
		Segment edge_1() const;
		Segment edge_2() const;
		Segment edge_3() const;

		Quad quad() const;

		RectangleF rectangle_rotated_to_axis() const;

		float angle() const;

		bool is_valid() const;

		bool operator==(const RectangleRotated& other) const;
		bool operator!=(const RectangleRotated& other) const;

	private:
		Point2F center_ = Point2F::ZERO;
		Vector2F x_axis_ = Vector2F::DIRECTION_RIGHT;
		Vector2F y_axis_ = Vector2F::DIRECTION_UP;
		Vector2F hw_extents_ = Vector2F::ZERO;

		// Four points of fixed size, in fixed storage. This was a
		// std::vector, so every construction, copy and assignment of an OBB
		// allocated, and each of the ten places that write a defining member
		// paid a malloc/free pair to refresh the cache. The argument is the
		// one already written above Shape for edges(); this member is where it
		// stopped one short.
		std::array<Point2F, 4> points_ = { Point2F::ZERO, Point2F::ZERO,
					Point2F::ZERO, Point2F::ZERO };

		std::array<Point2F, 4> calculate_points() const;

		std::array<mattmath::Point2F, 4> calculate_points(const mattmath::Point2F& center,
			const mattmath::Vector2F& x_axis, const mattmath::Vector2F& y_axis,
			const mattmath::Vector2F& hw_extents) const;

		bool half_widths_valid() const;
		bool axes_valid() const;
		bool edges_valid() const;
	};

	typedef RectangleRotated OBB;
}
