#pragma once

#include "engine/math/shape.h"
#include "engine/math/vector2f.h"
#include "engine/math/segment.h"
#include "engine/math/triangle.h"

#include <array>

namespace mattmath
{
	struct RectangleRotated;

	/*
	* A quadrilateral with four points.
	* The points are ordered in a clockwise direction, starting from the top left.
	*/
	struct Quad : public Shape
	{
		Quad() = default;
		Quad(const Quad&) = default;
		Quad(const Vector2F& point1, const Vector2F& point2,
			const Vector2F& point3, const Vector2F& point4);
		explicit Quad(const RectangleF& rectangle);
		explicit Quad(const RectangleRotated& rectangle);

		mattmath::RectangleF bounding_box() const override;
		ShapeType shape_type() const override;
		void offset(const mattmath::Vector2F& amount) override;
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

		Segment edge_0() const;
		Segment edge_1() const;
		Segment edge_2() const;
		Segment edge_3() const;
		std::array<Segment, 4> edges() const;

		Triangle triangle_0() const;
		Triangle triangle_1() const;
		std::array<Triangle, 2> triangles() const;

		bool operator==(const Quad& other) const;
		bool operator!=(const Quad& other) const;

		mattmath::Vector2F center() const override;

	private:
		Point2F points_[4] = { Vector2F::ZERO, Vector2F::ZERO,
					Vector2F::ZERO, Vector2F::ZERO };
	};
}
