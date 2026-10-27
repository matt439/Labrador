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
		RectangleI(const Vector2F& position,
			const Vector2F& size);
		explicit RectangleI(const RectangleF& rectangle);

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

		bool contains(const Vector2I& point) const;
		bool contains(const RectangleI& other) const;

		void offset(int horizontal_amount, int vertical_amount);
		void offset(const Vector2I& amount);

		void set_left(int left);
		void set_top(int top);
		void set_right(int right);
		void set_bottom(int bottom);

		void set_position(const Vector2I& position);
		void set_size(const Vector2I& size);

		void set_top_left(const Vector2I& top_left);
		void set_bottom_right(const Vector2I& bottom_right);
		void set_top_left_and_bottom_right(const Vector2I& top_left,
			const Vector2I& bottom_right);

		static const RectangleI zero;
	};
}
