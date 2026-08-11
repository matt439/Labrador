#include "engine/math/quad.h"

#include "engine/math/rectanglef.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/inflate.h"
#include "engine/math/ericson_math.h"

#include <array>
#include <stdexcept>

namespace mattmath
{

	Quad::Quad(const Vector2F& point0,
		const Vector2F& point1,
		const Vector2F& point2,
		const Vector2F& point3)
	{
		this->points_[0] = point0;
		this->points_[1] = point1;
		this->points_[2] = point2;
		this->points_[3] = point3;

		if (!this->is_valid())
		{
			throw std::invalid_argument("Quad is not valid");
		}
	}

	Quad::Quad(const RectangleF& rectangle)
	{
		this->points_[0] = Vector2F(rectangle.x, rectangle.y);
		this->points_[1] = Vector2F(rectangle.x + rectangle.width, rectangle.y);
		this->points_[2] = Vector2F(rectangle.x + rectangle.width, rectangle.y + rectangle.height);
		this->points_[3] = Vector2F(rectangle.x, rectangle.y + rectangle.height);

		if (!this->is_valid())
		{
			throw std::invalid_argument("Quad is not valid");
		}
	}

	Quad::Quad(const RectangleRotated& rectangle)
	{
		Quad q = rectangle.quad();

		this->points_[0] = q.point_0();
		this->points_[1] = q.point_1();
		this->points_[2] = q.point_2();
		this->points_[3] = q.point_3();

		if (!this->is_valid())
		{
			throw std::invalid_argument("Quad is not valid");
		}
	}

	RectangleF Quad::bounding_box() const
	{
		return RectangleF::bounding_box_of(this->points_);
	}

	ShapeType Quad::shape_type() const
	{
		return ShapeType::quad;
	}

	void Quad::offset(const Vector2F& offset)
	{
		this->points_[0] += offset;
		this->points_[1] += offset;
		this->points_[2] += offset;
		this->points_[3] += offset;
	}

	void Quad::inflate(float amount)
	{
		detail::inflate_convex_polygon(this->points_, 4, amount);
	}

	bool Quad::is_valid() const
	{
		// Convex, which is a stronger claim than the edges not crossing.
		//
		// Both consumers need convexity and neither needs simplicity: the
		// narrow phase runs the separating-axis theorem, which is only a
		// decision procedure for convex shapes, and triangles() splits on a
		// diagonal that lies outside a concave quad. A dart - four points with
		// one pushed back through the opposite diagonal - has no two edges
		// crossing, so it passed the old test and then received a confident
		// wrong manifold.
		//
		// A quad is convex exactly when its diagonals properly cross
		// (Ericson, 3.7.1): each diagonal must have the other's endpoints
		// strictly on opposite sides of it. Four signed areas, no allocation,
		// against six segment tests over a heap-allocated vector of edges.
		//
		// Strictness is what rejects the degenerate cases - three collinear
		// points, or a repeated vertex, put a zero on one side and a zero is
		// not strictly opposite anything.
		//
		// This also removes a trap. The old test asked whether any two edges
		// intersect, and adjacent edges share a vertex; it only passed at all
		// because segments_intersect excludes its endpoints. Closing that
		// boundary - which is an open item against segments_intersect - would
		// have made every Quad in the tree throw on construction.
		const Point2F& a = this->points_[0];
		const Point2F& b = this->points_[1];
		const Point2F& c = this->points_[2];
		const Point2F& d = this->points_[3];

		// a and c on opposite sides of diagonal bd
		if (!strictly_opposite_sides(Vector2F::cross(d - b, a - b),
			Vector2F::cross(d - b, c - b)))
		{
			return false;
		}

		// b and d on opposite sides of diagonal ac
		return strictly_opposite_sides(Vector2F::cross(c - a, b - a),
			Vector2F::cross(c - a, d - a));
	}

	const Point2F& Quad::point_0() const
	{
		return this->points_[0];
	}

	const Point2F& Quad::point_1() const
	{
		return this->points_[1];
	}

	const Point2F& Quad::point_2() const
	{
		return this->points_[2];
	}

	const Point2F& Quad::point_3() const
	{
		return this->points_[3];
	}

	// Each setter puts the point back if the result is not a valid quad, so a
	// rejected argument leaves the shape exactly as it was.
	//
	// RectangleRotated's setters carry the same note and were fixed for the
	// same reason: assigning to the member and throwing afterwards left the
	// object holding the value it had just refused. Here it was worse than
	// there, because is_valid() is a convexity test over all four points - so
	// a rejected set_point_1 left a quad that every subsequent setter would
	// also reject, and the shape could not be repaired through its own API.
	void Quad::set_point_0(const Point2F& point)
	{
		const Point2F previous = this->points_[0];
		this->points_[0] = point;

		if (!this->is_valid())
		{
			this->points_[0] = previous;
			throw std::invalid_argument("Quad is not valid");
		}
	}

	void Quad::set_point_1(const Point2F& point)
	{
		const Point2F previous = this->points_[1];
		this->points_[1] = point;

		if (!this->is_valid())
		{
			this->points_[1] = previous;
			throw std::invalid_argument("Quad is not valid");
		}
	}

	void Quad::set_point_2(const Point2F& point)
	{
		const Point2F previous = this->points_[2];
		this->points_[2] = point;

		if (!this->is_valid())
		{
			this->points_[2] = previous;
			throw std::invalid_argument("Quad is not valid");
		}
	}

	void Quad::set_point_3(const Point2F& point)
	{
		const Point2F previous = this->points_[3];
		this->points_[3] = point;

		if (!this->is_valid())
		{
			this->points_[3] = previous;
			throw std::invalid_argument("Quad is not valid");
		}
	}

	Segment Quad::edge_0() const
	{
		return Segment(this->points_[0], this->points_[1]);
	}

	Segment Quad::edge_1() const
	{
		return Segment(this->points_[1], this->points_[2]);
	}

	Segment Quad::edge_2() const
	{
		return Segment(this->points_[2], this->points_[3]);
	}

	Segment Quad::edge_3() const
	{
		return Segment(this->points_[3], this->points_[0]);
	}

	std::array<Segment, 4> Quad::edges() const
	{
		return
		{
			this->edge_0(),
			this->edge_1(),
			this->edge_2(),
			this->edge_3()
		};
	}

	Triangle Quad::triangle_0() const
	{
		return Triangle(this->points_[0], this->points_[1], this->points_[2]);
	}

	Triangle Quad::triangle_1() const
	{
		return Triangle(this->points_[0], this->points_[2], this->points_[3]);
	}

	std::array<Triangle, 2> Quad::triangles() const
	{
		return
		{
			this->triangle_0(),
			this->triangle_1()
		};
	}

	bool Quad::operator==(const Quad& other) const
	{
		return this->points_[0] == other.points_[0] &&
			this->points_[1] == other.points_[1] &&
			this->points_[2] == other.points_[2] &&
			this->points_[3] == other.points_[3];
	}

	bool Quad::operator!=(const Quad& other) const
	{
		return !(*this == other);
	}

	Point2F Quad::center() const
	{
		return (this->points_[0] + this->points_[1] + this->points_[2] + this->points_[3]) / 4.0f;
	}

}
