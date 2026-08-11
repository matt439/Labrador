#pragma once

#include "engine/math/shape.h"
#include "engine/math/vector2f.h"
#include "engine/math/segment.h"

#include <array>

namespace mattmath
{
	struct Triangle : public Shape
	{
		Vector2F points[3] = { Vector2F::ZERO, Vector2F::ZERO, Vector2F::ZERO };

		Triangle() = default;
		Triangle(const Triangle&) = default;
		Triangle(const mattmath::Vector2F& point0,
			const mattmath::Vector2F& point1,
			const mattmath::Vector2F& point2);
		Triangle(float x0, float y0, float x1, float y1, float x2, float y2);

		mattmath::RectangleF bounding_box() const override;
		ShapeType shape_type() const override;
		void offset(const mattmath::Vector2F& amount) override;
		void inflate(float amount) override;

		const Vector2F& point_0() const;
		const Vector2F& point_1() const;
		const Vector2F& point_2() const;

		Segment edge_0() const;
		Segment edge_1() const;
		Segment edge_2() const;
		std::array<Segment, 3> edges() const;

		float angle_0() const;
		float angle_1() const;
		float angle_2() const;

		bool operator==(const Triangle& other) const;
		bool operator!=(const Triangle& other) const;

		mattmath::Vector2F center() const override;

		float calculate_gradient(int edge) const;
	};
	struct TriangleRightAxisAligned : public Triangle
	{
		TriangleRightAxisAligned() = default;
		TriangleRightAxisAligned(const TriangleRightAxisAligned&) = default;
		TriangleRightAxisAligned(const mattmath::Vector2F& top,
			const mattmath::Vector2F& left, const mattmath::Vector2F& right);
		TriangleRightAxisAligned(float x0, float y0, float x1, float y1,
			float x2, float y2);

		Segment hypotenuse() const;
		float hypotenuse_gradient() const;

	private:
		// The gradient helper the base class already provides is NOT
		// redeclared here. It used to be, byte for byte, which hid the public
		// one behind a private copy - so a fix to the base would have missed
		// this type entirely.
		int find_hypotenuse(const Triangle& tri) const;
	};
}
