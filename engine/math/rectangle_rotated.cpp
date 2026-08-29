#include "engine/math/rectangle_rotated.h"

#include "engine/math/rectanglef.h"
#include "engine/math/quad.h"
#include "engine/math/scalar.h"
#include "engine/math/ericson_math.h"

#include <array>
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace mattmath
{

	RectangleRotated::RectangleRotated(const Point2F& center,
		const Vector2F& x_axis, const Vector2F& y_axis,
		const Vector2F& hw_extents) :
		center_(center), x_axis_(x_axis), y_axis_(y_axis), hw_extents_(hw_extents)
	{
		this->x_axis_.normalize();
		this->y_axis_.normalize();

		// Points first: is_valid() -> edges_valid() reads points_, so
		// validating before this line checked four zero corners and could
		// never reject anything.
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
		// Segment::direction() returns point_1 - point_0 un-normalised,
		// but axes_valid() requires a unit axis - without this the constructor
		// threw for every segment whose length was not 1.
		this->x_axis_ = center_line.direction();
		this->x_axis_.normalize();
		this->y_axis_ = Vector2F::normal(this->x_axis_);
		this->hw_extents_ = Vector2F(center_line.length() / 2.0f + thickness, thickness);

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
		// Validate the candidate before committing it. Assigning first and
		// throwing afterwards left the object holding the rejected value.
		const Vector2F inflated = this->hw_extents_ + Vector2F(amount, amount);
		if (inflated.x <= 0.0f || inflated.y <= 0.0f)
		{
			throw std::invalid_argument("Half widths are not valid");
		}

		this->hw_extents_ = inflated;
		this->points_ = this->calculate_points();
	}

	Point2F RectangleRotated::x_axis() const
	{
		return this->x_axis_;
	}
	Point2F RectangleRotated::y_axis() const
	{
		return this->y_axis_;
	}
	Point2F RectangleRotated::axis(int axis) const
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
	Point2F RectangleRotated::half_extents() const
	{
		return this->hw_extents_;
	}
	float RectangleRotated::half_x_width() const
	{
		return this->hw_extents_.x;
	}
	float RectangleRotated::half_y_width() const
	{
		return this->hw_extents_.y;
	}
	float RectangleRotated::half_width(int axis) const
	{
		if (axis == 0)
		{
			return this->hw_extents_.x;
		}
		else if (axis == 1)
		{
			return this->hw_extents_.y;
		}

		throw std::invalid_argument("Axis must be 0 or 1");
	}
	void RectangleRotated::set_center(const Point2F& center)
	{
		this->center_ = center;

		this->points_ = this->calculate_points();
	}
	// Each setter validates the candidate value before committing it, so a
	// rejected argument leaves the rectangle exactly as it was. Assigning to
	// the member first and throwing afterwards left the object corrupt: it
	// held the bad value and a stale points_ cache.
	void RectangleRotated::set_axes(const Point2F& x_axis, const Point2F& y_axis)
	{
		const Vector2F new_x = x_axis.normalized();
		const Vector2F new_y = y_axis.normalized();

		// Against each other, not against what the rectangle currently holds.
		// That is the whole point of this function: the single-axis setters
		// each validate a candidate against the partner they are not changing,
		// so a rotation is rejected in either order and the rectangle could
		// not be turned at all through its own API.
		if (!are_equal(new_x.length(), 1.0f, EPSILON) ||
			!are_equal(new_y.length(), 1.0f, EPSILON) ||
			!are_equal(Vector2F::dot(new_x, new_y), 0.0f, EPSILON))
		{
			throw std::invalid_argument("Axes are not valid");
		}

		this->x_axis_ = new_x;
		this->y_axis_ = new_y;
		this->points_ = this->calculate_points();
	}
	void RectangleRotated::set_x_axis(const Point2F& x_axis)
	{
		const Vector2F candidate = x_axis.normalized();

		if (!are_equal(candidate.length(), 1.0f, EPSILON) ||
			!are_equal(Vector2F::dot(candidate, this->y_axis_), 0.0f, EPSILON))
		{
			throw std::invalid_argument("Axes are not valid");
		}

		this->x_axis_ = candidate;
		this->points_ = this->calculate_points();
	}
	void RectangleRotated::set_y_axis(const Point2F& y_axis)
	{
		const Vector2F candidate = y_axis.normalized();

		if (!are_equal(candidate.length(), 1.0f, EPSILON) ||
			!are_equal(Vector2F::dot(this->x_axis_, candidate), 0.0f, EPSILON))
		{
			throw std::invalid_argument("Axes are not valid");
		}

		this->y_axis_ = candidate;
		this->points_ = this->calculate_points();
	}
	void RectangleRotated::set_half_extents(const Point2F& hw_extents)
	{
		if (hw_extents.x <= 0.0f || hw_extents.y <= 0.0f)
		{
			throw std::invalid_argument("Half widths are not valid");
		}

		this->hw_extents_ = hw_extents;
		this->points_ = this->calculate_points();
	}
	void RectangleRotated::set_half_x_width(float half_x_width)
	{
		if (half_x_width <= 0.0f)
		{
			throw std::invalid_argument("Half widths are not valid");
		}

		this->hw_extents_.x = half_x_width;
		this->points_ = this->calculate_points();
	}
	void RectangleRotated::set_half_y_width(float half_y_width)
	{
		if (half_y_width <= 0.0f)
		{
			throw std::invalid_argument("Half widths are not valid");
		}

		this->hw_extents_.y = half_y_width;
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
		// Straight off the corner cache, and NOT by recomputing the four points
		// into a fresh vector for a validating constructor: that allocates and
		// re-proves the convexity of a shape that is convex by construction.
		return Quad(this->points_[0], this->points_[1],
			this->points_[2], this->points_[3]);
	}
	RectangleF RectangleRotated::rectangle_rotated_to_axis() const
	{
		return RectangleF(this->center_, this->hw_extents_.x, this->hw_extents_.y);
	}
	float RectangleRotated::angle() const
	{
		// atan2 through Vector2F::angle(), not angle_between. angle_between is
		// an acos, so its range is [0, PI] and it cannot tell +30 degrees from
		// -30: a rectangle and its mirror image reported the same rotation,
		// and nothing could reconstruct the orientation from the number.
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

	// Declared in the header since the type was written, and never defined -
	// the one shape of the ten in this file whose comparison was a promise
	// only. Nothing had used it, so nothing had failed to link, and it read
	// from the header as though it worked.
	//
	// Compares the four members that DEFINE the rectangle, not points_, which
	// is a cache derived from them: two rectangles with equal centre, axes and
	// extents have equal corners by construction, and comparing the cache as
	// well would only add four ways to disagree with the thing that produced
	// it. Exact float equality, as every other shape here uses.
	bool RectangleRotated::operator==(const RectangleRotated& other) const
	{
		return this->center_ == other.center_ &&
			this->x_axis_ == other.x_axis_ &&
			this->y_axis_ == other.y_axis_ &&
			this->hw_extents_ == other.hw_extents_;
	}
	bool RectangleRotated::operator!=(const RectangleRotated& other) const
	{
		return !(*this == other);
	}

	std::array<Point2F, 4> RectangleRotated::calculate_points() const
	{
		return calculate_points(this->center_, this->x_axis_, this->y_axis_, this->hw_extents_);
	}

	std::array<Point2F, 4> RectangleRotated::calculate_points(const Point2F& center,
		const Vector2F& x_axis, const Vector2F& y_axis,
		const Vector2F& hw_extents) const
	{
		return
		{
			center - x_axis * hw_extents.x - y_axis * hw_extents.y,
			center + x_axis * hw_extents.x - y_axis * hw_extents.y,
			center + x_axis * hw_extents.x + y_axis * hw_extents.y,
			center - x_axis * hw_extents.x + y_axis * hw_extents.y
		};
	}

	bool RectangleRotated::half_widths_valid() const
	{
		// check that half width and half height are positive
		if (hw_extents_.x <= 0.0f || hw_extents_.y <= 0.0f)
		{
			return false;
		}

		return true;
	}
	bool RectangleRotated::axes_valid() const
	{
		// check if x_axis and y_axis are unit vectors
		if (!are_equal(x_axis_.length(), 1.0f, EPSILON) ||
			!are_equal(y_axis_.length(), 1.0f, EPSILON))
		{
			return false;
		}
		
		// check if x_axis and y_axis are perpendicular
		if (!are_equal(Vector2F::dot(x_axis_, y_axis_), 0.0f, EPSILON))
		{
			return false;
		}

		return true;
	}
	bool RectangleRotated::edges_valid() const
	{
		const auto edges = this->edges();

		// One pass, one square root per edge - against the twelve a loop that
		// normalises two edges per iteration and then re-measures all four with
		// length() takes, every edge normalised twice and every root computed
		// and thrown away once. No check on edges.size() either: edges() returns
		// a std::array<Segment, 4> and cannot answer anything else.
		Vector2F directions[4];
		float lengths[4];
		for (size_t i = 0; i < 4; i++)
		{
			const Vector2F edge = edges[i].direction();
			lengths[i] = edge.length();

			// Zero-length in, zero out - normalized()'s contract, kept
			// deliberately: a degenerate edge has always passed the
			// perpendicularity test below, because the dot product of two zero
			// vectors is zero.
			directions[i] = lengths[i] == 0.0f
				? Vector2F::ZERO
				: Vector2F(edge.x / lengths[i], edge.y / lengths[i]);
		}

		// Compare unit directions rather than the raw edge vectors. The dot
		// product of two un-normalised edges scales with the square of the
		// rectangle's size, so an absolute epsilon rejected large rectangles
		// that were perfectly square.
		for (size_t i = 0; i < 4; i++)
		{
			if (!are_equal(Vector2F::dot(directions[i], directions[(i + 1) % 4]),
				0.0f, EPSILON))
			{
				return false;
			}
		}

		// Opposite edges must match in length, to a tolerance that scales with
		// the edges themselves for the same reason.
		if (std::fabs(lengths[0] - lengths[2]) >
				EPSILON * std::max(1.0f, lengths[0]) ||
			std::fabs(lengths[1] - lengths[3]) >
				EPSILON * std::max(1.0f, lengths[1]))
		{
			return false;
		}

		return true;
	}

}
