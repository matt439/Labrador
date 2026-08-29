#pragma once

#include "engine/math/shape.h"
#include "engine/math/vector2f.h"
#include "engine/math/segment.h"

#include <array>
#include <span>

namespace mattmath
{
	struct RectangleI;

	struct RectangleF : public Shape
	{
		float x = 0.0f;
		float y = 0.0f;
		float width = 0.0f;
		float height = 0.0f;

		RectangleF() = default;
		RectangleF(const RectangleF&) = default;
		RectangleF(float x, float y, float width, float height);
		RectangleF(const mattmath::Vector2F& position,
							const mattmath::Vector2F& size);
		RectangleF(const mattmath::Vector2F& center, float horiz_half_width,
			float vert_half_height);

		RectangleF bounding_box() const override;
		ShapeType shape_type() const override;

		mattmath::Vector2F center() const override;
		mattmath::Vector2F position() const;
		mattmath::Vector2F size() const;
		float left() const;
		float right() const;
		float top() const;
		float bottom() const;
		mattmath::Vector2F top_left() const;
		mattmath::Vector2F bottom_right() const;
		mattmath::Vector2F top_right() const;
		mattmath::Vector2F bottom_left() const;
		mattmath::Segment top_edge() const;
		mattmath::Segment bottom_edge() const;
		mattmath::Segment left_edge() const;
		mattmath::Segment right_edge() const;
		std::array<mattmath::Segment, 4> edges() const;
		mattmath::RectangleI rectangle_i() const;

		bool operator==(const RectangleF& other) const;
		bool operator!=(const RectangleF& other) const;

		bool contains(const RectangleF& other) const;

		// The one member predicate, kept because broad_phase, scene and the
		// benchmark all name it. Every other pair is a free predicate above
		// (shape.h says why there is no virtual table over them).
		bool intersects(const RectangleF& other) const;

		void inflate(float horizontal_amount, float vertical_amount);
		void inflate(const mattmath::Vector2F& amount);
		void inflate(float amount) override;
		void offset(float horizontal_amount, float vertical_amount);
		void offset(const mattmath::Vector2F& amount) override;
		void scale(float horizontal_amount, float vertical_amount);
		void scale(const mattmath::Vector2F& amount);
		void set_position(const mattmath::Vector2F& position);
		void set_position(float x, float y);
		void set_position_at_center(const mattmath::Vector2F& position);
		void set_position_at_center(float x, float y);
		void set_position_x(float x);
		void set_position_x_from_right(float x);
		void set_position_y(float y);
		void set_position_y_from_bottom(float y);
		void set_position_from_top_right(const mattmath::Vector2F& position);
		void set_position_from_top_right(float x, float y);
		void set_position_from_bottom_left(const mattmath::Vector2F& position);
		void set_position_from_bottom_left(float x, float y);
		void set_position_from_bottom_right(const mattmath::Vector2F& position);
		void set_position_from_bottom_right(float x, float y);
		void set_size(const mattmath::Vector2F& size);
		void set_size(float width, float height);
		void set_width(float width);
		void set_height(float height);

		static RectangleF union_of(const RectangleF& a, const RectangleF& b);

		// The smallest axis-aligned box containing every point.
		//
		// Triangle, Quad and RectangleRotated each wrote this fold out; the
		// last two were character for character identical. Empty in, ZERO out
		// - a box around nothing.
		//
		// A non-finite coordinate is NOT rejected. std::min(a, NaN) returns a,
		// so a poisoned vertex is silently dropped rather than poisoning the
		// box, and checking per coordinate would put an isnan on a path every
		// broad-phase candidate walks. The predicates downstream are the ones
		// written to survive a NaN (see test_AABB_AABB); this stays cheap.
		static RectangleF bounding_box_of(std::span<const mattmath::Point2F> points);

		static RectangleF from_top_left_bottom_right(const mattmath::Vector2F& top_left,
			const mattmath::Vector2F& bottom_right);
		static RectangleF from_top_left_bottom_right(float top, float left,
			float bottom, float right);

		static const RectangleF ZERO;
	};

	typedef mattmath::RectangleF AABB;
}
