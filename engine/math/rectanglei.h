#pragma once

#include "engine/math/vector2i.h"

namespace mattmath
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
		RectangleI(int x, int y, int width, int height);
		RectangleI(const mattmath::Vector2I& position,
			const mattmath::Vector2I& size);
		RectangleI(const mattmath::Vector2F& position,
			const mattmath::Vector2F& size);
		explicit RectangleI(const mattmath::RectangleF& rectangle);

		int left() const;
		int top() const;
		int right() const;
		int bottom() const;

		mattmath::Vector2I position() const;
		mattmath::Vector2I size() const;

		mattmath::Vector2I top_left() const;
		mattmath::Vector2I bottom_right() const;

		bool operator==(const RectangleI& other) const;
		bool operator!=(const RectangleI& other) const;

		bool contains(const mattmath::Vector2I& point) const;
		bool contains(const RectangleI& other) const;

		void offset(int horizontal_amount, int vertical_amount);
		void offset(const mattmath::Vector2I& amount);

		void set_left(int left);
		void set_top(int top);
		void set_right(int right);
		void set_bottom(int bottom);

		void set_position(const mattmath::Vector2I& position);
		void set_size(const mattmath::Vector2I& size);

		void set_top_left(const mattmath::Vector2I& top_left);
		void set_bottom_right(const mattmath::Vector2I& bottom_right);
		void set_top_left_and_bottom_right(const mattmath::Vector2I& top_left,
			const mattmath::Vector2I& bottom_right);

		static const RectangleI ZERO;
	};
}
