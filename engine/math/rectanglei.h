#pragma once

#include "engine/math/vector2i.h"

namespace labrador
{
	struct Vector2F;
	struct RectangleF;

	struct RectangleI
	{
		int x = 0;
		int y = 0;
		int width = 0;
		int height = 0;

		RectangleI() = default;
		RectangleI(const RectangleI&) = default;
		RectangleI& operator=(const RectangleI&) = default;
		// constexpr and defined here so zero is constant-initialised. See
		// Vector2F's constants for what the dynamic alternative risks.
		constexpr RectangleI(int x, int y, int width, int height) :
			x(x), y(y), width(width), height(height) {}
		RectangleI(const Vector2I& position,
			const Vector2I& size);
		// Each of x, y, width and height truncates toward zero on its own,
		// as static_cast<int> does; the corners are not rounded outward, so
		// (0.5, 0.5, 2.9, 2.9) becomes (0, 0, 2, 2).
		RectangleI(const Vector2F& position,
			const Vector2F& size);
		explicit RectangleI(const RectangleF& rectangle);

		// right() is x + width and bottom() is y + height.
		int left() const;
		int top() const;
		int right() const;
		int bottom() const;

		Vector2I position() const;
		Vector2I size() const;

		Vector2I top_left() const;
		Vector2I bottom_right() const;

		bool operator==(const RectangleI& other) const;
		bool operator!=(const RectangleI& other) const;

		// Both are inclusive at all four edges: a point on right() or
		// bottom() is contained, and a rectangle contains itself.
		bool contains(const Vector2I& point) const;
		bool contains(const RectangleI& other) const;

		// offset and set_position move the rectangle and keep its size;
		// set_size keeps the top left.
		void offset(int horizontal_amount, int vertical_amount);
		void offset(const Vector2I& amount);
		void set_position(const Vector2I& position);
		void set_size(const Vector2I& size);

		static const RectangleI zero;
	};
}
