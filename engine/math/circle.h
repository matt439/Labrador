#pragma once

#include "engine/math/shape.h"
#include "engine/math/vector2f.h"

namespace mattmath
{
	struct Circle : public Shape
	{
		// Private, unlike the other shapes' data, because Shape's polymorphic
		// accessor is center() and a field of that name cannot coexist with
		// it. radius() is here to keep the pair symmetrical.

		Circle() = default;
		Circle(const Circle&) = default;
		Circle(const mattmath::Vector2F& center, float radius);
		Circle(float x, float y, float radius);

		mattmath::RectangleF bounding_box() const override;
		ShapeType shape_type() const override;
		void offset(const mattmath::Vector2F& amount) override;
		void inflate(float amount) override;

		bool operator==(const Circle& other) const;
		bool operator!=(const Circle& other) const;

		mattmath::Vector2F center() const override;
		float radius() const;

	private:
		mattmath::Vector2F center_ = mattmath::Vector2F::ZERO;
		float radius_ = 0.0f;
	};
}
