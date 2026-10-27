#pragma once

#include "engine/math/shape.h"
#include "engine/math/vector2f.h"

namespace labrador
{
	struct Circle : public Shape
	{
		// Private, unlike RectangleF's and Triangle's data, because Shape's
		// polymorphic accessor is center() and a field of that name cannot
		// coexist with it. radius() is here to keep the pair symmetrical.

		Circle() = default;
		Circle(const Circle&) = default;
		Circle& operator=(const Circle&) = default;
		Circle(const Vector2F& center, float radius);
		Circle(float x, float y, float radius);

		RectangleF bounding_box() const override;
		ShapeType shape_type() const override;
		void offset(const Vector2F& amount) override;
		void inflate(float amount) override;

		bool operator==(const Circle& other) const;
		bool operator!=(const Circle& other) const;

		Vector2F center() const override;
		float radius() const;

	private:
		Vector2F center_ = Vector2F::zero;
		float radius_ = 0.0f;
	};
}
