#pragma once

#include "engine/math/shape.h"
#include "engine/math/vector2f.h"
#include "engine/math/segment.h"
#include "engine/math/triangle.h"

#include <array>

namespace labrador
{
	struct RectangleRotated;

	// A convex quadrilateral: four points in order around it, either winding
	// and from any start, so long as the diagonals properly cross
	// (is_valid). The RectangleF form lists the top left, top right, bottom
	// right and bottom left corners.
	struct Quad : public Shape
	{
		Quad() = default;
		Quad(const Quad&) = default;
		Quad& operator=(const Quad&) = default;
		Quad(const Vector2F& point_0, const Vector2F& point_1,
			const Vector2F& point_2, const Vector2F& point_3);
		explicit Quad(const RectangleF& rectangle);
		explicit Quad(const RectangleRotated& rectangle);

		RectangleF bounding_box() const override;
		ShapeType shape_type() const override;
		void offset(const Vector2F& amount) override;
		void inflate(float amount) override;

		bool is_valid() const;

		const Point2F& point_0() const;
		const Point2F& point_1() const;
		const Point2F& point_2() const;
		const Point2F& point_3() const;
		void set_point_0(const Point2F& point);
		void set_point_1(const Point2F& point);
		void set_point_2(const Point2F& point);
		void set_point_3(const Point2F& point);
		// The four points, in the order point_0..point_3 name them.
		const std::array<Point2F, 4>& points() const;

		Segment edge_0() const;
		Segment edge_1() const;
		Segment edge_2() const;
		Segment edge_3() const;
		std::array<Segment, 4> edges() const;

		// Split along the diagonal from point 0 to point 2: (0, 1, 2) and
		// (0, 2, 3).
		std::array<Triangle, 2> triangles() const;

		bool operator==(const Quad& other) const;
		bool operator!=(const Quad& other) const;

		Vector2F center() const override;

	private:
		Triangle triangle_0() const;
		Triangle triangle_1() const;

		std::array<Point2F, 4> points_ = { Vector2F::zero, Vector2F::zero,
					Vector2F::zero, Vector2F::zero };
	};
}
