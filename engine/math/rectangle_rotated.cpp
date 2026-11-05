#include "engine/math/rectangle_rotated.h"

#include "engine/math/rectanglef.h"
#include "engine/math/quad.h"
#include "engine/math/scalar.h"
#include "engine/math/ericson_math.h"

#include <array>
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace labrador
{
	namespace
	{
		bool is_unit(const Vector2F& axis)
		{
			return are_equal(axis.length(), 1.0f, default_epsilon);
		}

		bool are_perpendicular(const Vector2F& a, const Vector2F& b)
		{
			return are_equal(Vector2F::dot(a, b), 0.0f, default_epsilon);
		}

		// Written as the rejection, so that a NaN extent, which fails every
		// comparison, is not mistaken for a positive one.
		bool not_positive(float half_width)
		{
			return half_width <= 0.0f;
		}

		bool any_not_positive(const Vector2F& half_extents)
		{
			return not_positive(half_extents.x) || not_positive(half_extents.y);
		}
	}

	RectangleRotated::RectangleRotated(const Point2F& center,
		const Vector2F& x_axis, const Vector2F& y_axis,
		const Vector2F& half_extents) :
		center_(center), x_axis_(x_axis), y_axis_(y_axis), half_extents_(half_extents)
	{
		this->x_axis_.normalize();
		this->y_axis_.normalize();

		// edges_valid() reads the corner cache, so populate it before checking
		// the rectangle's geometry.
		this->points_ = this->calculate_points();

		if (!this->is_valid())
		{
			throw std::invalid_argument("RectangleRotated is not valid");
		}
	}
	RectangleRotated::RectangleRotated(const Segment& center_line,
		float thickness)
	{
		if (center_line.length() == 0.0f)
		{
			throw std::invalid_argument(
				"RectangleRotated: centre line has zero length, so it has no direction");
		}

		this->center_ = center_line.center();
		// Segment::direction() is unnormalised; axes_valid() requires unit axes.
		this->x_axis_ = center_line.direction();
		this->x_axis_.normalize();
		this->y_axis_ = Vector2F::normal(this->x_axis_);
		this->half_extents_ = Vector2F(center_line.length() / 2.0f + thickness, thickness);

		this->points_ = this->calculate_points();

		if (!this->is_valid())
		{
			throw std::invalid_argument("RectangleRotated is not valid");
		}
	}

	RectangleF RectangleRotated::bounding_box() const
	{
		return RectangleF::bounding_box_of(this->points_);
	}
	ShapeType RectangleRotated::shape_type() const
	{
		return ShapeType::rectangle_rotated;
	}
	void RectangleRotated::offset(const Vector2F& amount)
	{
		this->center_ += amount;

		this->points_ = this->calculate_points();

	}
	Point2F RectangleRotated::center() const
	{
		return this->center_;
	}
	std::array<Segment, 4> RectangleRotated::edges() const
	{
		return
		{
			this->edge_0(),
			this->edge_1(),
			this->edge_2(),
			this->edge_3()
		};
	}
	void RectangleRotated::inflate(float amount)
	{
		// Reject nonpositive extents before changing the defining values or cache.
		const Vector2F inflated = this->half_extents_ + Vector2F(amount, amount);
		if (any_not_positive(inflated))
		{
			throw std::invalid_argument("Half widths are not valid");
		}

		this->half_extents_ = inflated;
		this->points_ = this->calculate_points();
	}

	Vector2F RectangleRotated::x_axis() const
	{
		return this->x_axis_;
	}
	Vector2F RectangleRotated::y_axis() const
	{
		return this->y_axis_;
	}
	Vector2F RectangleRotated::axis(int axis) const
	{
		if (axis == 0)
		{
			return this->x_axis_;
		}
		else if (axis == 1)
		{
			return this->y_axis_;
		}

		throw std::invalid_argument("Axis must be 0 or 1");
	}
	Vector2F RectangleRotated::half_extents() const
	{
		return this->half_extents_;
	}
	float RectangleRotated::half_x_width() const
	{
		return this->half_extents_.x;
	}
	float RectangleRotated::half_y_width() const
	{
		return this->half_extents_.y;
	}
	float RectangleRotated::half_width(int axis) const
	{
		if (axis == 0)
		{
			return this->half_extents_.x;
		}
		else if (axis == 1)
		{
			return this->half_extents_.y;
		}

		throw std::invalid_argument("Axis must be 0 or 1");
	}
	void RectangleRotated::set_center(const Point2F& center)
	{
		this->center_ = center;

		this->points_ = this->calculate_points();
	}
	// Validate candidates before committing them so rejection preserves both
	// the defining members and the corner cache.
	void RectangleRotated::set_axes(const Vector2F& x_axis, const Vector2F& y_axis)
	{
		const Vector2F new_x = x_axis.normalized();
		const Vector2F new_y = y_axis.normalized();

		// Validate the candidate axes together. A single-axis setter requires
		// perpendicularity to the unchanged partner and cannot turn the pair.
		if (!is_unit(new_x) || !is_unit(new_y) ||
			!are_perpendicular(new_x, new_y))
		{
			throw std::invalid_argument("Axes are not valid");
		}

		this->x_axis_ = new_x;
		this->y_axis_ = new_y;
		this->points_ = this->calculate_points();
	}
	void RectangleRotated::set_x_axis(const Vector2F& x_axis)
	{
		const Vector2F candidate = x_axis.normalized();

		if (!is_unit(candidate) ||
			!are_perpendicular(candidate, this->y_axis_))
		{
			throw std::invalid_argument("Axes are not valid");
		}

		this->x_axis_ = candidate;
		this->points_ = this->calculate_points();
	}
	void RectangleRotated::set_y_axis(const Vector2F& y_axis)
	{
		const Vector2F candidate = y_axis.normalized();

		if (!is_unit(candidate) ||
			!are_perpendicular(this->x_axis_, candidate))
		{
			throw std::invalid_argument("Axes are not valid");
		}

		this->y_axis_ = candidate;
		this->points_ = this->calculate_points();
	}
	void RectangleRotated::set_half_extents(const Vector2F& half_extents)
	{
		if (any_not_positive(half_extents))
		{
			throw std::invalid_argument("Half widths are not valid");
		}

		this->half_extents_ = half_extents;
		this->points_ = this->calculate_points();
	}
	void RectangleRotated::set_half_x_width(float half_x_width)
	{
		if (not_positive(half_x_width))
		{
			throw std::invalid_argument("Half widths are not valid");
		}

		this->half_extents_.x = half_x_width;
		this->points_ = this->calculate_points();
	}
	void RectangleRotated::set_half_y_width(float half_y_width)
	{
		if (not_positive(half_y_width))
		{
			throw std::invalid_argument("Half widths are not valid");
		}

		this->half_extents_.y = half_y_width;
		this->points_ = this->calculate_points();
	}
	Point2F RectangleRotated::point_0() const
	{
		return this->points_[0];
	}
	Point2F RectangleRotated::point_1() const
	{
		return this->points_[1];
	}
	Point2F RectangleRotated::point_2() const
	{
		return this->points_[2];
	}
	Point2F RectangleRotated::point_3() const
	{
		return this->points_[3];
	}
	const std::array<Point2F, 4>& RectangleRotated::points() const
	{
		return this->points_;
	}
	Segment RectangleRotated::edge_0() const
	{
		return Segment(this->points_[0], this->points_[1]);
	}
	Segment RectangleRotated::edge_1() const
	{
		return Segment(this->points_[1], this->points_[2]);
	}
	Segment RectangleRotated::edge_2() const
	{
		return Segment(this->points_[2], this->points_[3]);
	}
	Segment RectangleRotated::edge_3() const
	{
		return Segment(this->points_[3], this->points_[0]);
	}
	Quad RectangleRotated::quad() const
	{
		// Pass the cached corners directly to Quad's fixed-storage constructor.
		return Quad(this->points_[0], this->points_[1],
			this->points_[2], this->points_[3]);
	}
	RectangleF RectangleRotated::rectangle_rotated_to_axis() const
	{
		return RectangleF(this->center_, this->half_extents_.x, this->half_extents_.y);
	}
	float RectangleRotated::angle() const
	{
		// atan2 preserves the signed orientation; an acos angle between axes
		// cannot distinguish positive and negative turns.
		return this->x_axis_.angle();
	}
	bool RectangleRotated::is_valid() const
	{
		if (!this->half_widths_valid())
		{
			return false;
		}
		if (!this->axes_valid())
		{
			return false;
		}
		if (!this->edges_valid())
		{
			return false;
		}

		return true;
	}

	// Exact comparison of the defining centre, axes and extents. points_ is a
	// derived cache, so it does not contribute an independent equality rule.
	bool RectangleRotated::operator==(const RectangleRotated& other) const
	{
		return this->center_ == other.center_ &&
			this->x_axis_ == other.x_axis_ &&
			this->y_axis_ == other.y_axis_ &&
			this->half_extents_ == other.half_extents_;
	}
	bool RectangleRotated::operator!=(const RectangleRotated& other) const
	{
		return !(*this == other);
	}

	std::array<Point2F, 4> RectangleRotated::calculate_points() const
	{
		return calculate_points(this->center_, this->x_axis_, this->y_axis_, this->half_extents_);
	}

	std::array<Point2F, 4> RectangleRotated::calculate_points(const Point2F& center,
		const Vector2F& x_axis, const Vector2F& y_axis,
		const Vector2F& half_extents) const
	{
		return
		{
			center - x_axis * half_extents.x - y_axis * half_extents.y,
			center + x_axis * half_extents.x - y_axis * half_extents.y,
			center + x_axis * half_extents.x + y_axis * half_extents.y,
			center - x_axis * half_extents.x + y_axis * half_extents.y
		};
	}

	bool RectangleRotated::half_widths_valid() const
	{
		return !any_not_positive(this->half_extents_);
	}
	bool RectangleRotated::axes_valid() const
	{
		return is_unit(this->x_axis_) && is_unit(this->y_axis_) &&
			are_perpendicular(this->x_axis_, this->y_axis_);
	}
	bool RectangleRotated::edges_valid() const
	{
		const std::array<Segment, 4> edges = this->edges();

		// Reuse each edge's length for its unit direction and opposite-edge
		// comparison. edges() returns fixed storage for the four edges.
		Vector2F directions[4];
		float lengths[4];
		for (size_t i = 0; i < 4; i++)
		{
			const Vector2F edge = edges[i].direction();
			lengths[i] = edge.length();

			// Preserve normalized()'s zero-vector result without dividing by zero.
			// Perpendicularity below does not reject a zero direction: its dot
			// product with a finite adjacent direction is zero.
			directions[i] = lengths[i] == 0.0f
				? Vector2F::zero
				: Vector2F(edge.x / lengths[i], edge.y / lengths[i]);
		}

		// Unit-direction dot products test perpendicularity independently of
		// edge length; raw-edge products scale with rectangle size squared.
		for (size_t i = 0; i < 4; i++)
		{
			if (!are_equal(Vector2F::dot(directions[i], directions[(i + 1) % 4]),
				0.0f, default_epsilon))
			{
				return false;
			}
		}

		// Opposite edges must match in length, to a tolerance that scales with
		// the edges themselves for the same reason.
		if (std::fabs(lengths[0] - lengths[2]) >
				default_epsilon * std::max(1.0f, lengths[0]) ||
			std::fabs(lengths[1] - lengths[3]) >
				default_epsilon * std::max(1.0f, lengths[1]))
		{
			return false;
		}

		return true;
	}

}
