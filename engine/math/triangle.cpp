#include "engine/math/triangle.h"

#include "engine/math/rectanglef.h"
#include "engine/math/scalar.h"
#include "engine/math/inflate.h"
#include "engine/math/ericson_math.h"

#include <array>
#include <cmath>
#include <stdexcept>
#include <span>

namespace mattmath
{

	Triangle::Triangle(const Vector2F& point0,
		const Vector2F& point1,
		const Vector2F& point2)
	{
		this->points[0] = point0;
		this->points[1] = point1;
		this->points[2] = point2;
	}
	Triangle::Triangle(float x0, float y0, float x1, float y1, float x2, float y2)
	{
		this->points[0] = Vector2F(x0, y0);
		this->points[1] = Vector2F(x1, y1);
		this->points[2] = Vector2F(x2, y2);
	}
	RectangleF Triangle::bounding_box() const
	{
		return RectangleF::bounding_box_of(this->points);
	}
	ShapeType Triangle::shape_type() const
	{
		return ShapeType::triangle;
	}
	void Triangle::offset(const Vector2F& offset)
	{
		this->points[0] += offset;
		this->points[1] += offset;
		this->points[2] += offset;
	}

	void Triangle::inflate(float amount)
	{
		detail::inflate_convex_polygon(this->points, 3, amount);
	}
	const Vector2F& Triangle::point_0() const
	{
		return this->points[0];
	}
	const Vector2F& Triangle::point_1() const
	{
		return this->points[1];
	}
	const Vector2F& Triangle::point_2() const
	{
		return this->points[2];
	}
	Segment Triangle::edge_0() const
	{
		return Segment(this->points[0], this->points[1]);
	}
	Segment Triangle::edge_1() const
	{
		return Segment(this->points[1], this->points[2]);
	}
	Segment Triangle::edge_2() const
	{
		return Segment(this->points[2], this->points[0]);
	}
	std::array<Segment, 3> Triangle::edges() const
	{
		return
		{
			this->edge_0(),
			this->edge_1(),
			this->edge_2()
		};
	}
	// The INTERIOR angle at each vertex - the one a reader means by "the
	// angles of a triangle", and the one that makes angles() sum to PI.
	//
	// NOT the angle between the two edge DIRECTIONS meeting at the vertex,
	// which is a different quantity: edge_2 runs p2 -> p0 while the interior
	// angle at p0 is measured from p0 outwards, so that gives PI minus the
	// interior angle and makes angles() sum to 2*PI. A right angle is its own
	// supplement, so find_hypotenuse cannot detect the difference.
	float Triangle::angle_0() const
	{
		return Vector2F::angle_between(this->points[1] - this->points[0],
			this->points[2] - this->points[0]);
	}
	float Triangle::angle_1() const
	{
		return Vector2F::angle_between(this->points[0] - this->points[1],
			this->points[2] - this->points[1]);
	}
	float Triangle::angle_2() const
	{
		return Vector2F::angle_between(this->points[0] - this->points[2],
			this->points[1] - this->points[2]);
	}
	bool Triangle::operator==(const Triangle& other) const
	{
		return this->points[0] == other.points[0] &&
			this->points[1] == other.points[1] &&
			this->points[2] == other.points[2];
	}
	bool Triangle::operator!=(const Triangle& other) const
	{
		return !(*this == other);
	}

	Point2F Triangle::center() const
	{
		return (this->points[0] + this->points[1] + this->points[2]) / 3.0f;
	}
	
	float Triangle::calculate_gradient(int edge) const
	{
		if (edge < 0 || edge > 2)
		{
			throw std::invalid_argument("Edge must be between 0 and 2");
		}

		return (this->points[(edge + 1) % 3].y - this->points[edge].y) /
			(this->points[(edge + 1) % 3].x - this->points[edge].x);
	}


	TriangleRightAxisAligned::TriangleRightAxisAligned(
		const Vector2F& top,
		const Vector2F& left, const Vector2F& right) :
		Triangle(top, left, right)
	{

	}

	TriangleRightAxisAligned::TriangleRightAxisAligned(
		float x0, float y0, float x1, float y1, float x2, float y2) :
		Triangle(x0, y0, x1, y1, x2, y2)
	{

	}

	namespace
	{
		// find_hypotenuse answers with a VERTEX index - the corner holding the
		// right angle - and the hypotenuse is the side that does not touch it.
		// Since edge n runs from vertex n to vertex n + 1, the side opposite
		// vertex v is edge v + 1.
		//
		// Both accessors below used the vertex index as an edge index
		// directly, so both returned a leg. For the right triangle
		// (0,0), (0,10), (10,0) the right angle is at vertex 0 and the
		// hypotenuse runs (0,10) -> (10,0) with length 14.14; they returned
		// (0,0) -> (0,10), length 10, and a gradient to match.
		int edge_opposite_vertex(int vertex)
		{
			return (vertex + 1) % 3;
		}
	}

	Segment TriangleRightAxisAligned::hypotenuse() const
	{
		int right_angle_vertex = this->find_hypotenuse(*this);
		if (right_angle_vertex == -1)
		{
			throw std::invalid_argument("Triangle is not a right triangle");
		}

		return this->edges()[edge_opposite_vertex(right_angle_vertex)];
	}
	float TriangleRightAxisAligned::hypotenuse_gradient() const
	{
		int right_angle_vertex = this->find_hypotenuse(*this);
		if (right_angle_vertex == -1)
		{
			throw std::invalid_argument("Triangle is not a right triangle");
		}

		return this->calculate_gradient(
			edge_opposite_vertex(right_angle_vertex));
	}

	//bool TriangleRightAxisAligned::is_right_triangle(const Triangle& tri) const
	//{
	//	int hypotenuse = this->find_hypotenuse(tri);
	//	if (hypotenuse == -1)
	//	{
	//		return false;
	//	}

	//	return true;
	//}

	int TriangleRightAxisAligned::find_hypotenuse(const Triangle& tri) const
	{
		// find the hypotenuse
		if (are_equal(tri.angle_0(), PI_OVER_2))
		{
			return 0;
		}
		else if (are_equal(tri.angle_1(), PI_OVER_2))
		{
			return 1;
		}
		else if (are_equal(tri.angle_2(), PI_OVER_2))
		{
			return 2;
		}
		else
		{
			return -1;
		}
	}
}
