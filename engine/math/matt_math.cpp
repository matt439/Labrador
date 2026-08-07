#include "engine/math/matt_math.h"
#include "engine/math/ericson_math.h"

#include <stdexcept>

using namespace DirectX;

namespace mattmath
{

#pragma region Misc

	//float mattmath::min_value(float a, float b)
	//{
	//	if (a < b)
	//	{
	//		return a;
	//	}
	//	else
	//	{
	//		return b;
	//	}
	//}
	//float mattmath::max_value(float a, float b)
	//{
	//	if (a > b)
	//	{
	//		return a;
	//	}
	//	else
	//	{
	//		return b;
	//	}
	//}
	float mattmath::clamp(float value, float min, float max)
	{
		if (value < min)
		{
			return min;
		}
		else if (value > max)
		{
			return max;
		}
		else
		{
			return value;
		}
	}
	void mattmath::clamp_ref(float& value, float min, float max)
	{
		if (value < min)
		{
			value = min;
		}
		else if (value > max)
		{
			value = max;
		}
	}
	int mattmath::clamp(int value, int min, int max)
	{
		if (value < min)
		{
			return min;
		}
		else if (value > max)
		{
			return max;
		}
		else
		{
			return value;
		}
	}
	void mattmath::clamp_ref(int& value, int min, int max)
	{
		if (value < min)
		{
			value = min;
		}
		else if (value > max)
		{
			value = max;
		}
	}
	int mattmath::sign(const Vector2F& p1,
		const Vector2F& p2, const Vector2F& p3)
	{
		return static_cast<int>((p1.x - p3.x) * (p2.y - p3.y) -
			(p2.x - p3.x) * (p1.y - p3.y));
	}

	bool mattmath::are_equal(float a, float b, float epsilon)
	{
		return fabs(a - b) < epsilon;
	}
	bool mattmath::are_equal(const Vector2F& a, const Vector2F& b, float epsilon)
	{
		return are_equal(a.x, b.x, epsilon) && are_equal(a.y, b.y, epsilon);
	}
	float mattmath::to_radians(float degrees)
	{
		return degrees * (PI / 180.0f);
	}
	float mattmath::to_degrees(float radians)
	{
		return radians * (180.0f / PI);
	}

	float mattmath::lerp(float a, float b, float t)
	{
		return a + (b - a) * t;
	}

#pragma endregion Misc

#pragma region Shape

	bool Shape::intersects(const Shape* other) const
	{
		switch (other->get_shape_type())
		{
		case shape_type::RECTANGLE:
			return this->intersects(*dynamic_cast<const RectangleF*>(other));
		case shape_type::CIRCLE:
			return this->intersects(*dynamic_cast<const Circle*>(other));
		case shape_type::TRIANGLE:
			return this->intersects(*dynamic_cast<const Triangle*>(other));
		case shape_type::QUAD:
			return this->intersects(*dynamic_cast<const Quad*>(other));
		case shape_type::RECTANGLE_ROTATED:
			return this->intersects(*dynamic_cast<const RectangleRotated*>(other));
		default:
			throw std::invalid_argument("Shape type not recognized");
		};
	}

	bool Shape::intersects(const Shape& other) const
	{
		return this->intersects(&other);
	}

	bool Shape::AABB_intersects(const Shape* other) const
	{
		return this->get_bounding_box().intersects(other->get_bounding_box());
	}

	bool Shape::AABB_intersects(const Shape& other) const
	{
		return this->AABB_intersects(&other);
	}

#pragma endregion Shape

#pragma region Global Intersect Functions

	bool mattmath::shapes_intersect(const Shape* a, const Shape* b)
	{
		return a->intersects(b);
	}

	bool mattmath::shapes_intersect(const Shape& a, const Shape& b)
	{
		return a.intersects(b);
	}

	bool mattmath::shapes_AABB_intersect(const Shape* a, const Shape* b)
	{
		return a->get_bounding_box().intersects(b->get_bounding_box());
	}

	bool mattmath::shapes_AABB_intersect(const Shape& a, const Shape& b)
	{
		return a.get_bounding_box().intersects(b.get_bounding_box());
	}

	bool rectangles_intersect(const RectangleF& a, const RectangleF& b)
	{
		return EricsonMath::test_AABB_AABB(a, b);
	}

	bool mattmath::rectangle_circle_intersect(const RectangleF& rectangle, const Circle& circle,
		Point2F& point)
	{
		return EricsonMath::test_circle_AABB(circle, rectangle, point);
	}

	bool mattmath::rectangle_circle_intersect(const RectangleF& rectangle, const Circle& circle)
	{
		return EricsonMath::test_circle_AABB(circle, rectangle);
	}

	bool mattmath::rectangle_triangle_intersect(const RectangleF& rectangle, const Triangle& triangle)
	{
		// AABB vs AABB
		if (!rectangle.intersects(triangle.get_bounding_box()))
		{
			return false;
		}

		// check if the rectangle contains any of the triangle's points
		if (rectangle.contains(triangle.get_point_0()) ||
			rectangle.contains(triangle.get_point_1()) ||
			rectangle.contains(triangle.get_point_2()))
		{
			return true;
		}

		// check if the triangle contains any of the rectangle's points
		if (triangle.contains(rectangle.get_top_left()) ||
			triangle.contains(rectangle.get_top_right()) ||
			triangle.contains(rectangle.get_bottom_left()) ||
			triangle.contains(rectangle.get_bottom_right()))
		{
			return true;
		}

		// check if any of the triangle's edges intersect the rectangle
		std::vector<Segment> edges = triangle.get_edges();
		for (const Segment& edge : edges)
		{
			if (rectangle.intersects(edge))
			{
				return true;
			}
		}

		return false;
	}

	bool mattmath::rectangle_quad_intersect(const RectangleF& rectangle, const Quad& quad)
	{
		// get the triangles of the quad
		std::vector<Triangle> triangles = quad.get_triangles();

		// check each triangle against the rectangle
		for (const Triangle& triangle : triangles)
		{
			if (rectangle_triangle_intersect(rectangle, triangle))
			{
				return true;
			}
		}
		return false;
	}

	bool mattmath::rectangle_segment_intersect(const RectangleF& rectangle, const Segment& segment)
	{
		return EricsonMath::test_segment_AABB(segment.point_0, segment.point_1, rectangle);
	}

	bool mattmath::rectangle_point_intersect(const RectangleF& rectangle, const Point2F& point)
	{
		return point.x >= rectangle.x &&
			point.x <= rectangle.x + rectangle.width &&
			point.y >= rectangle.y &&
			point.y <= rectangle.y + rectangle.height;
	}

	bool mattmath::rectangle_rotated_rectangle_intersect(const RectangleF& rect,
		const RectangleRotated& rotated_rect)
	{
		// check if the rectangles' bounding boxes intersect
		if (!rect.intersects(rotated_rect.get_bounding_box()))
		{
			return false;
		}

		// check if the rotated rectangle contains any of the rectangle's points
		if (rotated_rect.contains(rect.get_top_left()) ||
			rotated_rect.contains(rect.get_top_right()) ||
			rotated_rect.contains(rect.get_bottom_left()) ||
			rotated_rect.contains(rect.get_bottom_right()))
		{
			return true;
		}

		// check if the rectangle contains any of the rotated rectangle's points
		if (rect.contains(rotated_rect.get_point_0()) ||
			rect.contains(rotated_rect.get_point_1()) ||
			rect.contains(rotated_rect.get_point_2()) ||
			rect.contains(rotated_rect.get_point_3()))
		{
			return true;
		}

		// check if any of the rotated rectangle's edges intersect the rectangle
		std::vector<Segment> edges = rotated_rect.get_edges();
		for (const Segment& edge : edges)
		{
			if (rect.intersects(edge))
			{
				return true;
			}
		}

		return false;
	}

	bool mattmath::circles_intersect(const Circle& a, const Circle& b)
	{
		return Vector2F::distance(a.center, b.center) <=
			a.radius + b.radius;
	}

	bool mattmath::circle_triangle_intersect(const Circle& circle, const Triangle& triangle, Point2F& point)
	{
		return EricsonMath::test_circle_triangle(circle, triangle.get_point_0(),
			triangle.get_point_1(), triangle.get_point_2(), point);
	}

	bool mattmath::circle_triangle_intersect(const Circle& circle, const Triangle& triangle)
	{
		Point2F point;
		return EricsonMath::test_circle_triangle(circle, triangle.get_point_0(),
			triangle.get_point_1(), triangle.get_point_2(), point);
	}

	bool mattmath::circle_quad_intersect(const Circle& circle, const Quad& quad)
	{
		std::vector<Triangle> triangles = quad.get_triangles();

		for (const Triangle& triangle : triangles)
		{
			if (circle_triangle_intersect(circle, triangle))
			{
				return true;
			}
		}
		return false;
	}

	bool mattmath::circle_segment_intersect(const Circle& circle, const Segment& segment, Point2F& point)
	{
		float t;
		EricsonMath::closest_pt_point_segment(circle.center,
			segment.point_0, segment.point_1, t, point);

		return Vector2F::distance(circle.center, point) <= circle.radius;
	}

	bool mattmath::circle_segment_intersect(const Circle& circle, const Segment& segment)
	{
		Point2F point;
		return circle_segment_intersect(circle, segment, point);
	}

	bool mattmath::circle_point_intersect(const Circle& circle, const Point2F& point)
	{
		return Vector2F::distance(circle.center, point) <= circle.radius;
	}

	bool mattmath::circle_rectangle_rotated_intersect(const Circle& circle,
		const RectangleRotated& rect_rotated)
	{
		// check if the circle intersects the rectangle's bounding box
		if (!circle.intersects(rect_rotated.get_bounding_box()))
		{
			return false;
		}

		// check if circle's center is contained within the rectangle
		if (rect_rotated.contains(circle.center))
		{
			return true;
		}

		// check if the circle intersects any of the rectangle's edges
		std::vector<Segment> edges = rect_rotated.get_edges();
		for (const Segment& edge : edges)
		{
			if (circle_segment_intersect(circle, edge))
			{
				return true;
			}
		}

		return false;
	}

	bool mattmath::triangles_intersect(const Triangle& a, const Triangle& b)
	{
		// check if any of the points are contained within each other
		for (int i = 0; i < 3; i++)
		{
			if (a.contains(b.points[i]) || b.contains(a.points[i]))
			{
				return true;
			}
		}

		// get edges of each triangle
		std::vector<Segment> a_edges = a.get_edges();
		std::vector<Segment> b_edges = b.get_edges();

		// check if any of the edges intersect
		for (int i = 0; i < 2; i++)
		{
			if (segments_intersect(a_edges[i], b_edges[0]) ||
				segments_intersect(a_edges[i], b_edges[1]))
			{
				return true;
			}
		}

		return false;
	}

	bool mattmath::triangle_quad_intersect(const Triangle& triangle, const Quad& quad)
	{
		// get the triangles of the quad
		std::vector<Triangle> triangles = quad.get_triangles();

		// check each triangle against the quad's triangles
		for (const Triangle& quad_triangle : triangles)
		{
			if (triangles_intersect(triangle, quad_triangle))
			{
				return true;
			}
		}

		return false;
	}

	bool mattmath::triangle_segment_intersect(const Triangle& triangle, const Segment& segment)
	{
		// check if the segment's end points are contained within the triangle
		if (triangle.contains(segment.point_0) || triangle.contains(segment.point_1))
		{
			return true;
		}

		// check if the segment intersects any of the triangle's edges
		std::vector<Segment> edges = triangle.get_edges();
		for (const Segment& edge : edges)
		{
			if (segments_intersect(edge, segment))
			{
				return true;
			}
		}

		return false;
	}

	bool mattmath::triangle_point_intersect(const Triangle& triangle, const Point2F& point)
	{
		return EricsonMath::test_point_triangle(point, triangle.points[0],
			triangle.points[1], triangle.points[2]);
	}

	bool mattmath::triangle_rectangle_rotated_intersect(const Triangle& triangle,
		const RectangleRotated& rect_rotated)
	{
		// check if the triangle intersects the rectangle's bounding box
		if (!triangle.intersects(rect_rotated.get_bounding_box()))
		{
			return false;
		}

		// check if the triangle contains any of the rectangle's points
		if (triangle.contains(rect_rotated.get_point_0()) ||
			triangle.contains(rect_rotated.get_point_1()) ||
			triangle.contains(rect_rotated.get_point_2()) ||
			triangle.contains(rect_rotated.get_point_3()))
		{
			return true;
		}

		// check if the rectangle contains any of the triangle's points
		if (rect_rotated.contains(triangle.points[0]) ||
			rect_rotated.contains(triangle.points[1]) ||
			rect_rotated.contains(triangle.points[2]))
		{
			return true;
		}

		// check if any of the triangle's edges intersect the rectangle
		std::vector<Segment> edges = triangle.get_edges();
		for (const Segment& edge : edges)
		{
			if (rect_rotated.intersects(edge))
			{
				return true;
			}
		}

		return false;
	}

	bool mattmath::quads_intersect(const Quad& a, const Quad& b)
	{
		// get the triangles of each quad
		std::vector<Triangle> a_triangles = a.get_triangles();
		std::vector<Triangle> b_triangles = b.get_triangles();

		// check each triangle of one quad against the other
		for (const Triangle& a_triangle : a_triangles)
		{
			for (const Triangle& b_triangle : b_triangles)
			{
				if (triangles_intersect(a_triangle, b_triangle))
				{
					return true;
				}
			}
		}

		return false;
	}

	bool mattmath::quad_segment_intersect(const Quad& quad, const Segment& segment)
	{
		// get the triangles of the quad
		std::vector<Triangle> triangles = quad.get_triangles();

		// check each triangle against the segment
		for (const Triangle& triangle : triangles)
		{
			if (triangle_segment_intersect(triangle, segment))
			{
				return true;
			}
		}

		return false;
	}

	bool mattmath::quad_point_intersect(const Quad& quad, const Point2F& point)
	{
		// check if the point is contained within any of the quad's triangles
		std::vector<Triangle> triangles = quad.get_triangles();
		for (const Triangle& triangle : triangles)
		{
			if (triangle.contains(point))
			{
				return true;
			}
		}

		return false;
	}

	bool mattmath::quad_rectangle_rotated_intersect(const Quad& quad,
		const RectangleRotated& rect_rotated)
	{
		// get the triangles of the quad
		std::vector<Triangle> triangles = quad.get_triangles();

		// check each triangle against the rotated rectangle
		for (const Triangle& triangle : triangles)
		{
			if (triangle_rectangle_rotated_intersect(triangle, rect_rotated))
			{
				return true;
			}
		}

		return false;
	}

	bool mattmath::segments_intersect(const Segment& a, const Segment& b, float& t, Point2F& p)
	{
		if (a == b)
		{
			t = 0.0f;
			p = a.point_0;
			return true;
		}
		
		return EricsonMath::test_2D_segment_segment(a.point_0, a.point_1,
			b.point_0, b.point_1, t, p);
	}

	bool mattmath::segments_intersect(const Segment& a, const Segment& b)
	{
		float t;
		Point2F p;
		return segments_intersect(a, b, t, p);
	}

	bool mattmath::segment_rectangle_rotated_intersect(const Segment& segment,
		const RectangleRotated& rect_rotated)
	{
		// check if the segment intersects the rectangle's bounding box
		if (!segment.intersects(rect_rotated.get_bounding_box()))
		{
			return false;
		}

		// check if the segment's end points are contained within the rectangle
		if (rect_rotated.contains(segment.point_0) || rect_rotated.contains(segment.point_1))
		{
			return true;
		}

		// check if the segment intersects any of the rectangle's edges
		std::vector<Segment> edges = rect_rotated.get_edges();
		for (const Segment& edge : edges)
		{
			if (segments_intersect(segment, edge))
			{
				return true;
			}
		}

		return false;
	}

	bool mattmath::point_rectangle_rotated_intersect(const Point2F& point,
		const RectangleRotated& rect_rotated)
	{
		Quad quad = rect_rotated.get_quad();

		return quad_point_intersect(quad, point);
	}

	bool mattmath::rectangles_rotated_intersect(const RectangleRotated& a,
		const RectangleRotated& b)
	{
		// check if the rectangles' bounding boxes intersect
		if (!a.intersects(b.get_bounding_box()))
		{
			return false;
		}

		// check if any of the points of one rectangle are contained within the other
		if (a.contains(b.get_point_0()) ||
			a.contains(b.get_point_1()) ||
			a.contains(b.get_point_2()) ||
			a.contains(b.get_point_3()))
		{
			return true;
		}

		if (b.contains(a.get_point_0()) ||
			b.contains(a.get_point_1()) ||
			b.contains(a.get_point_2()) ||
			b.contains(a.get_point_3()))
		{
			return true;
		}

		// check if any of the edges of one rectangle intersect the other
		std::vector<Segment> a_edges = a.get_edges();
		std::vector<Segment> b_edges = b.get_edges();

		for (const Segment& edge : a_edges)
		{
			for (const Segment& other_edge : b_edges)
			{
				if (segments_intersect(edge, other_edge))
				{
					return true;
				}
			}
		}

		return false;
	}

#pragma endregion Global Intersect Functions

#pragma region RectangleF

	RectangleF::RectangleF(float x, float y, float width, float height) :
		x(x), y(y), width(width), height(height)
	{
	}
	RectangleF::RectangleF(const Vector2F& position,
		const Vector2F& size)
	{
		this->x = position.x;
		this->y = position.y;
		this->width = size.x;
		this->height = size.y;
	}
	RectangleF::RectangleF(const SimpleMath::Vector2& position, const SimpleMath::Vector2& size)
	{
		this->x = position.x;
		this->y = position.y;
		this->width = size.x;
		this->height = size.y;
	}
	RectangleF::RectangleF(const SimpleMath::Rectangle& rectangle)
	{
		this->x = static_cast<float>(rectangle.x);
		this->y = static_cast<float>(rectangle.y);
		this->width = static_cast<float>(rectangle.width);
		this->height = static_cast<float>(rectangle.height);
	}
	RectangleF::RectangleF(const RECT& rectangle)
	{
		this->x = static_cast<float>(rectangle.left);
		this->y = static_cast<float>(rectangle.top);
		this->width = static_cast<float>(rectangle.right - rectangle.left);
		this->height = static_cast<float>(rectangle.bottom - rectangle.top);
	}
	RectangleF::RectangleF(const Vector2F& center, float horiz_half_width,
		float vert_half_height)
	{
		this->x = center.x - horiz_half_width;
		this->y = center.y - vert_half_height;
		this->width = horiz_half_width * 2.0f;
		this->height = vert_half_height * 2.0f;
	}
	//RectangleF::RectangleF(const Segment& center_line, float thickness)
	//{
	//	Vector2F center = center_line.get_center();
	//	Vector2F direction = center_line.get_direction();
	//	Vector2F perpendicular = Vector2F(-direction.y, direction.x);
	//	Vector2F half_thickness = perpendicular * thickness / 2.0f;
	//	this->x = center.x - half_thickness.x;
	//	this->y = center.y - half_thickness.y;
	//	this->width = thickness;
	//	this->height = thickness;
	//}
	RectangleF RectangleF::get_bounding_box() const
	{
		return *this;
	}
	shape_type RectangleF::get_shape_type() const
	{
		return shape_type::RECTANGLE;
	}
	std::unique_ptr<Shape> RectangleF::clone() const
	{
		return std::make_unique<RectangleF>(*this);
	}
	float RectangleF::get_x() const
	{
		return this->x;
	}
	float RectangleF::get_y() const
	{
		return this->y;
	}
	float RectangleF::get_width() const
	{
		return this->width;
	}
	float RectangleF::get_height() const
	{
		return this->height;
	}
	Vector2F RectangleF::get_center() const
	{
		return Vector2F(this->x + this->width / 2.0f,
					this->y + this->height / 2.0f);
	}
	Vector2F RectangleF::get_position() const
	{
		return Vector2F(this->x, this->y);
	}
	Vector2F RectangleF::get_size() const
	{
		return Vector2F(this->width, this->height);
	}
	float RectangleF::get_left() const
	{
		return this->x;
	}
	float RectangleF::get_right() const
	{
		return this->x + this->width;
	}
	float RectangleF::get_top() const
	{
		return this->y;
	}
	float RectangleF::get_bottom() const
	{
		return this->y + this->height;
	}
	Vector2F RectangleF::get_top_left() const
	{
		return Vector2F(this->x, this->y);
	}
	Vector2F RectangleF::get_bottom_right() const
	{
		return Vector2F(this->x + this->width, this->y + this->height);
	}
	Vector2F RectangleF::get_top_right() const
	{
		return Vector2F(this->x + this->width, this->y);
	}
	Vector2F RectangleF::get_bottom_left() const
	{
		return Vector2F(this->x, this->y + this->height);
	}
	Segment RectangleF::get_top_edge() const
	{
		return Segment(this->get_top_left(), this->get_top_right());
	}
	Segment RectangleF::get_bottom_edge() const
	{
		return Segment(this->get_bottom_left(), this->get_bottom_right());
	}
	Segment RectangleF::get_left_edge() const
	{
		return Segment(this->get_top_left(), this->get_bottom_left());
	}
	Segment RectangleF::get_right_edge() const
	{
		return Segment(this->get_top_right(), this->get_bottom_right());
	}
	std::vector<Segment> RectangleF::get_edges() const
	{
		std::vector<Segment> edges = 
		{
			this->get_top_edge(),
			this->get_bottom_edge(),
			this->get_left_edge(),
			this->get_right_edge()
		};
		return edges;
	}
	float RectangleF::get_area() const
	{
		return this->width * this->height;
	}
	RectangleI RectangleF::get_rectangle_i() const
	{
		return RectangleI(static_cast<int>(this->x),
			static_cast<int>(this->y),
			static_cast<int>(this->width),
			static_cast<int>(this->height));
	}
	SimpleMath::Rectangle RectangleF::get_sm_rectangle() const
	{
		return SimpleMath::Rectangle(static_cast<long>(this->x),
					static_cast<long>(this->y),
					static_cast<long>(this->width),
					static_cast<long>(this->height));
	}
	RECT RectangleF::get_win_rect() const
	{
		RECT result = {
			static_cast<long>(this->x),
			static_cast<long>(this->y),
			static_cast<long>(this->x + this->width),
			static_cast<long>(this->y + this->height)
		};
		return result;
	}
	//RectangleF& RectangleF::operator=(const Rectangle& rectangle)
	//{
	//	this->x = static_cast<float>(rectangle.x);
	//	this->y = static_cast<float>(rectangle.y);
	//	this->width = static_cast<float>(rectangle.width);
	//	this->height = static_cast<float>(rectangle.height);
	//	return *this;
	//}
	//RectangleF& RectangleF::operator=(const RECT& rectangle)
	//{
	//	this->x = static_cast<float>(rectangle.left);
	//	this->y = static_cast<float>(rectangle.top);
	//	this->width = static_cast<float>(rectangle.right - rectangle.left);
	//	this->height = static_cast<float>(rectangle.bottom - rectangle.top);
	//	return *this;
	//}
	bool RectangleF::operator==(const RectangleF& other) const
	{
		return this->x == other.x &&
			this->y == other.y &&
			this->width == other.width &&
			this->height == other.height;
	}
	bool RectangleF::operator!=(const RectangleF& other) const
	{
		return !(*this == other);
	}
	//bool RectangleF::contains(const Vector2F& point) const
	//{
	//	return point.x >= this->x &&
	//		point.x <= this->x + this->width &&
	//		point.y >= this->y &&
	//		point.y <= this->y + this->height;
	//}
	bool RectangleF::contains(const RectangleF& other) const
	{
		return other.x >= this->x &&
			other.x + other.width <= this->x + this->width &&
			other.y >= this->y &&
			other.y + other.height <= this->y + this->height;
	}
	bool RectangleF::intersects(const RectangleF& other) const
	{
		return rectangles_intersect(*this, other);
	}
	bool RectangleF::intersects(const Circle& other) const
	{
		return rectangle_circle_intersect(*this, other);
	}
	bool RectangleF::intersects(const Triangle& other) const
	{
		return rectangle_triangle_intersect(*this, other);
	}
	bool RectangleF::intersects(const Quad& other) const
	{
		return rectangle_quad_intersect(*this, other);
	}
	bool RectangleF::intersects(const Segment& other) const
	{
		return rectangle_segment_intersect(*this, other);
	}
	bool RectangleF::intersects(const Point2F& other) const
	{
		return rectangle_point_intersect(*this, other);
	}
	bool RectangleF::contains(const Point2F& point) const
	{
		return this->intersects(point);
	}
	bool RectangleF::intersects(const RectangleRotated& other) const
	{
		return rectangle_rotated_rectangle_intersect(*this, other);
	}
	RectangleF RectangleF::intersection(const RectangleF& other) const
	{
		return RectangleF::intersection(*this, other);
	}
	//void RectangleF::set_left(float left)
	//{
	//	this->width += this->x - left;
	//	this->x = left;
	//}
	//void RectangleF::set_top(float top);
	//void RectangleF::set_right(float right);
	//void RectangleF::set_bottom(float bottom);
	void RectangleF::inflate(float horizontal_amount, float vertical_amount)
	{
		this->x -= horizontal_amount;
		this->y -= vertical_amount;
		this->width += horizontal_amount * 2.0f;
		this->height += vertical_amount * 2.0f;
	}
	void RectangleF::inflate(const Vector2F& amount)
	{
		this->x -= amount.x;
		this->y -= amount.y;
		this->width += amount.x * 2.0f;
		this->height += amount.y * 2.0f;
	}
	void RectangleF::inflate(float amount)
	{
		this->inflate(amount, amount);
	}
	void RectangleF::inflate_to_size(float new_width, float new_height)
	{
		this->x -= (new_width - this->width) / 2.0f;
		this->y -= (new_height - this->height) / 2.0f;
		this->width = new_width;
		this->height = new_height;
	}
	void RectangleF::inflate_to_size(const Vector2F& size)
	{
		this->x -= (size.x - this->width) / 2.0f;
		this->y -= (size.y - this->height) / 2.0f;
		this->width = size.x;
		this->height = size.y;
	}
	void RectangleF::scale_at_center(float scale)
	{
		this->scale_at_center(scale, scale);
	}
	void RectangleF::scale_at_center(float horizontal_scale, float vertical_scale)
	{
		Vector2F center = this->get_center();
		this->x = center.x - this->width * horizontal_scale / 2.0f;
		this->y = center.y - this->height * vertical_scale / 2.0f;
		this->width *= horizontal_scale;
		this->height *= vertical_scale;
	}
	void RectangleF::scale_at_center(const Vector2F& scale)
	{
		this->scale_at_center(scale.x, scale.y);
	}
	void RectangleF::offset(float horizontal_amount, float vertical_amount)
	{
		this->x += horizontal_amount;
		this->y += vertical_amount;
	}
	void RectangleF::offset(const mattmath::Vector2F& amount)
	{
		this->x += amount.x;
		this->y += amount.y;
	}
	void RectangleF::scale(float horizontal_amount, float vertical_amount)
	{
		this->width *= horizontal_amount;
		this->height *= vertical_amount;
	}
	void RectangleF::scale(const Vector2F& amount)
	{
		this->width *= amount.x;
		this->height *= amount.y;
	}
	void RectangleF::scale_size_and_position(float horizontal_amount,
		float vertical_amount)
	{
		this->x *= horizontal_amount;
		this->y *= vertical_amount;
		this->width *= horizontal_amount;
		this->height *= vertical_amount;
	}
	void RectangleF::scale_size_and_position(const Vector2F& amount)
	{
		this->x *= amount.x;
		this->y *= amount.y;
		this->width *= amount.x;
		this->height *= amount.y;
	}
	void RectangleF::set_position(const Vector2F& position)
	{
		this->x = position.x;
		this->y = position.y;
	}
	void RectangleF::set_position(float new_x, float new_y)
	{
		this->x = new_x;
		this->y = new_y;
	}
	void RectangleF::set_position_at_center(const Vector2F& position)
	{
		this->x = position.x - this->width / 2.0f;
		this->y = position.y - this->height / 2.0f;
	}
	void RectangleF::set_position_at_center(float new_x, float new_y)
	{
		this->x = new_x - this->width / 2.0f;
		this->y = new_y - this->height / 2.0f;
	}
	void RectangleF::set_position_x(float new_x)
	{
		this->x = new_x;
	}
	void RectangleF::set_position_x_from_right(float new_x)
	{
		this->x = new_x - this->width;
	}
	void RectangleF::set_position_y(float new_y)
	{
		this->y = new_y;
	}
	void RectangleF::set_position_y_from_bottom(float new_y)
	{
		this->y = new_y - this->height;
	}
	void RectangleF::set_position_from_top_right(const Vector2F& position)
	{
		this->x = position.x - this->width;
		this->y = position.y;
	}
	void RectangleF::set_position_from_top_right(float new_x, float new_y)
	{
		this->x = new_x - this->width;
		this->y = new_y;
	}
	void RectangleF::set_position_from_bottom_left(const Vector2F& position)
	{
		this->x = position.x;
		this->y = position.y - this->height;
	}
	void RectangleF::set_position_from_bottom_left(float new_x, float new_y)
	{
		this->x = new_x;
		this->y = new_y - this->height;
	}
	void RectangleF::set_position_from_bottom_right(const Vector2F& position)
	{
		this->x = position.x - this->width;
		this->y = position.y - this->height;
	}
	void RectangleF::set_position_from_bottom_right(float new_x, float new_y)
	{
		this->x = new_x - this->width;
		this->y = new_y - this->height;
	}
	void RectangleF::set_size(const Vector2F& size)
	{
		this->width = size.x;
		this->height = size.y;
	}
	void RectangleF::set_size(float new_width, float new_height)
	{
		this->width = new_width;
		this->height = new_height;
	}
	void RectangleF::set_width(float new_width)
	{
		this->width = new_width;
	}
	void RectangleF::set_height(float new_height)
	{
		this->height = new_height;
	}
	RectangleF RectangleF::intersection(const RectangleF& a, const RectangleF& b)
	{
		float x1 = std::max(a.x, b.x);
		float x2 = std::min(a.x + a.width, b.x + b.width);
		float y1 = std::max(a.y, b.y);
		float y2 = std::min(a.y + a.height, b.y + b.height);
		if (x2 >= x1 && y2 >= y1)
		{
			return RectangleF(x1, y1, x2 - x1, y2 - y1);
		}
		else
		{
			return RectangleF();
		}
	}
	RectangleF RectangleF::union_of(const RectangleF& a, const RectangleF& b)
	{
		float x1 = std::min(a.x, b.x);
		float x2 = std::max(a.x + a.width, b.x + b.width);
		float y1 = std::min(a.y, b.y);
		float y2 = std::max(a.y + a.height, b.y + b.height);
		return RectangleF(x1, y1, x2 - x1, y2 - y1);
	}
	RectangleF RectangleF::from_top_left_bottom_right(const Vector2F& top_left,
		const Vector2F& bottom_right)
	{
		return RectangleF(top_left.x, top_left.y,
			bottom_right.x - top_left.x,
			bottom_right.y - top_left.y);
	}
	RectangleF RectangleF::from_top_left_bottom_right(float top, float left,
		float bottom, float right)
	{
		return RectangleF(left, top, right - left, bottom - top);
	}
	const RectangleF RectangleF::ZERO = { 0.0f, 0.0f, 0.0f, 0.0f };

#pragma endregion RectangleF

#pragma region Matrix

	//template<typename T>
	//Matrix<T>::Matrix(int rows, int columns)
	//{
	//	this->rows_ = rows;
	//	this->columns_ = columns;
	//	this->elements_ = std::vector<T>(rows * columns);
	//}
	//
	//template<typename T>
	//Matrix<T>::Matrix(int rows, int columns, const std::vector<T>& elements)
	//{
	//	this->rows_ = rows;
	//	this->columns_ = columns;
	//	if (rows * columns != elements.size())
	//	{
	//		throw std::invalid_argument(
	//			"Number of elements does not match the dimensions of the matrix");
	//	}
	//	this->elements_ = elements;
	//}
	//
	//template<typename T>
	//Matrix<T>::Matrix(int size, vector_type type)
	//{
	//	if (type == vector_type::ROW)
	//	{
	//		this->rows_ = 1;
	//		this->columns_ = size;
	//	}
	//	else
	//	{
	//		this->rows_ = size;
	//		this->columns_ = 1;
	//	}
	//	this->elements_ = std::vector<T>(size);
	//}
	//
	//template<typename T>
	//Matrix<T>::Matrix(int size, vector_type type, const std::vector<T>& elements)
	//{
	//	if (type == vector_type::ROW)
	//	{
	//		this->rows_ = 1;
	//		this->columns_ = size;
	//	}
	//	else
	//	{
	//		this->rows_ = size;
	//		this->columns_ = 1;
	//	}
	//	if (size != elements.size())
	//	{
	//		throw std::invalid_argument(
	//			"Number of elements does not match the dimensions of the matrix");
	//	}
	//	this->elements_ = elements;
	//}
	//
	//template<typename T>
	//T Matrix<T>::get_element(int row, int column) const
	//{
	//	// check if the row and column are valid
	//	if (!row_valid(row) || !column_valid(column))
	//	{
	//		throw std::invalid_argument("Row or column is not valid");
	//	}
	//	int index = calculate_index(row, column);
	//	return this->elements_[index];
	//}
	//
	//template<typename T>
	//void Matrix<T>::set_element(int row, int column, T value)
	//{
	//	// check if the row and column are valid
	//	if (!row_valid(row) || !column_valid(column))
	//	{
	//		throw std::invalid_argument("Row or column is not valid");
	//	}
	//	int index = calculate_index(row, column);
	//	this->elements_[index] = value;
	//}
	//
	//template<typename T>
	//int Matrix<T>::get_rows() const
	//{
	//	return this->rows_;
	//}
	//
	//template<typename T>
	//int Matrix<T>::get_columns() const
	//{
	//	return this->columns_;
	//}
	//
	//template<typename T>
	//bool Matrix<T>::is_square() const
	//{
	//	return this->rows_ == this->columns_;
	//}
	//
	//template<typename T>
	//bool Matrix<T>::equal_size(const Matrix<T>& other) const
	//{
	//	return this->rows_ == other.get_rows() && this->columns_ == other.get_columns();
	//}
	//
	//template<typename T>
	//Matrix<T> Matrix<T>::rotate_pi_radians() const
	//{
	//	// create a new matrix with the same dimensions
	//	Matrix<T> rotated_matrix(this->rows_, this->columns_);
	//
	//	// rotate the matrix by 180 degrees
	//	for (int i = 0; i < this->rows_; i++)
	//	{
	//		for (int j = 0; j < this->columns_; j++)
	//		{
	//			rotated_matrix(i, j) = this->elements_[(this->rows_ - i - 1) * this->columns_ + (this->columns_ - j - 1)];
	//		}
	//	}
	//
	//	return rotated_matrix;
	//}
	//
	//template<typename T>
	//T& Matrix<T>::operator()(int row, int column)
	//{
	//	return this->get_element_ref(row, column);
	//}
	//
	//template<typename T>
	//const T& Matrix<T>::operator()(int row, int column) const
	//{
	//	return this->get_element(row, column);
	//}
	//
	//template<typename T>
	//bool Matrix<T>::row_valid(int row) const
	//{
	//	return row >= 0 && row < this->rows_;
	//}
	//
	//template<typename T>
	//bool Matrix<T>::column_valid(int column) const
	//{
	//	return column >= 0 && column < this->columns_;
	//}
	//
	//template<typename T>
	//int Matrix<T>::calculate_index(int row, int column) const
	//{
	//	return row * this->columns_ + column;
	//}
	//
	//template<typename T>
	//T& Matrix<T>::get_element_ref(int row, int column)
	//{
	//	// check if the row and column are valid
	//	if (!row_valid(row) || !column_valid(column))
	//	{
	//		throw std::invalid_argument("Row or column is not valid");
	//	}
	//	int index = calculate_index(row, column);
	//	return this->elements_[index];
	//}

#pragma endregion Matrix

#pragma region Vector

	//template<typename T>
	//Vector<T>::Vector(int size) :
	//	Matrix<T>(size, vector_type::COLUMN)
	//{
	//}
	//
	//template<typename T>
	//Vector<T>::Vector(int size, vector_type type) :
	//	Matrix<T>(size, type)
	//{
	//}
	//
	//template<typename T>
	//Vector<T>::Vector(int size, vector_type type, const std::vector<T>& elements) :
	//	Matrix<T>(size, type, elements)
	//{
	//}
	//
	//template<typename T>
	//int Vector<T>::get_size() const
	//{
	//	if (this->get_vector_type() == vector_type::ROW)
	//	{
	//		return this->get_columns();
	//	}
	//	else
	//	{
	//		return this->get_rows();
	//	}
	//}
	//
	//template<typename T>
	//vector_type Vector<T>::get_vector_type() const
	//{
	//	if (this->get_rows() == 1)
	//	{
	//		return vector_type::ROW;
	//	}
	//	else
	//	{
	//		return vector_type::COLUMN;
	//	}
	//}
	//
	//template<typename T>
	//T& Vector<T>::operator[](int index)
	//{
	//	return this->get_element_ref(index, 0);
	//}
	//
	//template<typename T>
	//const T& Vector<T>::operator[](int index) const
	//{
	//	return this->get_element(index, 0);
	//}

#pragma endregion Vector

#pragma region Vector2I

	Vector2I::Vector2I(int x, int y)
	{
		this->x = x;
		this->y = y;
	}
	Vector2I::Vector2I(const Vector2F& vector)
	{
		this->x = static_cast<int>(vector.x);
		this->y = static_cast<int>(vector.y);
	}
	Vector2I::Vector2I(const SimpleMath::Vector2& vector)
	{
		this->x = static_cast<int>(vector.x);
		this->y = static_cast<int>(vector.y);
	}
	Vector2I::Vector2I(const XMINT2& vector)
	{
		this->x = vector.x;
		this->y = vector.y;
	}

	SimpleMath::Vector2 Vector2I::get_sm_vector() const
	{
		return SimpleMath::Vector2(static_cast<float>(this->x),
					static_cast<float>(this->y));
	}
	XMINT2 Vector2I::get_xm_vector() const
	{
		return XMINT2(this->x, this->y);
	}

	Vector2I& Vector2I::operator=(const SimpleMath::Vector2& vector)
	{
		this->x = static_cast<int>(vector.x);
		this->y = static_cast<int>(vector.y);
		return *this;
	}
	Vector2I& Vector2I::operator=(const XMINT2& vector)
	{
		this->x = vector.x;
		this->y = vector.y;
		return *this;
	}
	bool Vector2I::operator==(const Vector2I& other) const
	{
		return this->x == other.x && this->y == other.y;
	}
	bool Vector2I::operator!=(const Vector2I& other) const
	{
		return !(*this == other);
	}
	Vector2I& Vector2I::operator+=(const Vector2I& other)
	{
		this->x += other.x;
		this->y += other.y;
		return *this;
	}
	Vector2I& Vector2I::operator-=(const Vector2I& other)
	{
		this->x -= other.x;
		this->y -= other.y;
		return *this;
	}
	Vector2I& Vector2I::operator*=(const Vector2I& other)
	{
		this->x *= other.x;
		this->y *= other.y;
		return *this;
	}
	Vector2I& Vector2I::operator/=(const Vector2I& other)
	{
		this->x /= other.x;
		this->y /= other.y;
		return *this;
	}
	Vector2I& Vector2I::operator*=(int other)
	{
		this->x *= other;
		this->y *= other;
		return *this;
	}
	Vector2I& Vector2I::operator/=(int other)
	{
		this->x /= other;
		this->y /= other;
		return *this;
	}
	//Vector2I Vector2I::operator+(const Vector2I& other) const
	//{
	//	return Vector2I(this->x + other.x, this->y + other.y);
	//}
	//Vector2I Vector2I::operator-(const Vector2I& other) const
	//{
	//	return Vector2I(this->x - other.x, this->y - other.y);
	//}
	void Vector2I::offset(int horizontal_amount, int vertical_amount)
	{
		this->x += horizontal_amount;
		this->y += vertical_amount;
	}
	void Vector2I::scale(int horizontal_amount, int vertical_amount)
	{
		this->x *= horizontal_amount;
		this->y *= vertical_amount;
	}
	void Vector2I::scale(const DirectX::SimpleMath::Vector2& amount)
	{
		this->x *= static_cast<int>(amount.x);
		this->y *= static_cast<int>(amount.y);
	}
	void Vector2I::set(int new_x, int new_y)
	{
		this->x = new_x;
		this->y = new_y;
	}
	void Vector2I::set(const SimpleMath::Vector2& vector)
	{
		this->x = static_cast<int>(vector.x);
		this->y = static_cast<int>(vector.y);
	}
	void Vector2I::set(const XMINT2& vector)
	{
		this->x = vector.x;
		this->y = vector.y;
	}
	const Vector2I Vector2I::ZERO = { 0, 0 };
	Vector2I mattmath::operator+ (const Vector2I& V1, const Vector2I& V2)
	{
		return Vector2I(V1.x + V2.x, V1.y + V2.y);
	}
	Vector2I mattmath::operator- (const Vector2I& V1, const Vector2I& V2)
	{
		return Vector2I(V1.x - V2.x, V1.y - V2.y);
	}
	Vector2I mattmath::operator* (const Vector2I& V1, const Vector2I& V2)
	{
		return Vector2I(V1.x * V2.x, V1.y * V2.y);
	}
	Vector2I mattmath::operator* (const Vector2I& V, int S)
	{
		return Vector2I(V.x * S, V.y * S);
	}
	Vector2I mattmath::operator/ (const Vector2I& V1, const Vector2I& V2)
	{
		return Vector2I(V1.x / V2.x, V1.y / V2.y);
	}
	Vector2I mattmath::operator/ (const Vector2I& V, int S)
	{
		return Vector2I(V.x / S, V.y / S);
	}
	Vector2I mattmath::operator* (int S, const Vector2I& V)
	{
		return Vector2I(V.x * S, V.y * S);
	}

#pragma endregion Vector2I

#pragma region Vector2F

	Vector2F::Vector2F(float f)
	{
		this->x = f;
		this->y = f;
	}
	Vector2F::Vector2F(float x, float y)
	{
		this->x = x;
		this->y = y;
	}
	Vector2F::Vector2F(const SimpleMath::Vector2& vector)
	{
		this->x = vector.x;
		this->y = vector.y;
	}
	Vector2F::Vector2F(const XMFLOAT2& vector)
	{
		this->x = vector.x;
		this->y = vector.y;
	}
	Vector2F::Vector2F(const Vector2I& vector)
	{
		this->x = static_cast<float>(vector.x);
		this->y = static_cast<float>(vector.y);
	}
	SimpleMath::Vector2 Vector2F::get_sm_vector() const
	{
		return SimpleMath::Vector2(this->x, this->y);
	}
	XMFLOAT2 Vector2F::get_xm_vector() const
	{
		return XMFLOAT2(this->x, this->y);
	}
	Vector2F& Vector2F::operator=(const SimpleMath::Vector2& vector)
	{
		this->x = vector.x;
		this->y = vector.y;
		return *this;
	}
	Vector2F& Vector2F::operator=(const XMFLOAT2& vector)
	{
		this->x = vector.x;
		this->y = vector.y;
		return *this;
	}
	bool Vector2F::operator==(const Vector2F& other) const
	{
		return this->x == other.x && this->y == other.y;
	}
	bool Vector2F::operator!=(const Vector2F& other) const
	{
		return !(*this == other);
	}
	Vector2F& Vector2F::operator+=(const Vector2F& other)
	{
		this->x += other.x;
		this->y += other.y;
		return *this;
	}
	Vector2F& Vector2F::operator-=(const Vector2F& other)
	{
		this->x -= other.x;
		this->y -= other.y;
		return *this;
	}
	Vector2F& Vector2F::operator*=(const Vector2F& other)
	{
		this->x *= other.x;
		this->y *= other.y;
		return *this;
	}
	Vector2F& Vector2F::operator/=(const Vector2F& other)
	{
		this->x /= other.x;
		this->y /= other.y;
		return *this;
	}
	Vector2F& Vector2F::operator*=(float other)
	{
		this->x *= other;
		this->y *= other;
		return *this;
	}
	Vector2F& Vector2F::operator/=(float other)
	{
		this->x /= other;
		this->y /= other;
		return *this;
	}
	//Vector2F& Vector2F::operator/(float other)
	//{
	//	this->x /= other;
	//	this->y /= other;
	//	return *this;
	//}
	//Vector2F& Vector2F::operator*(float other)
	//{
	//	this->x *= other;
	//	this->y *= other;
	//	return *this;
	//}
	//Vector2F& Vector2F::operator+(const Vector2F& other)
	//{
	//	this->x += other.x;
	//	this->y += other.y;
	//	return *this;
	//}
	//Vector2F& Vector2F::operator-(const Vector2F& other)
	//{
	//	this->x -= other.x;
	//	this->y -= other.y;
	//	return *this;
	//}
	//Vector2F& Vector2F::operator*(const Vector2F& other)
	//{
	//	this->x *= other.x;
	//	this->y *= other.y;
	//	return *this;
	//}
	//Vector2F& Vector2F::operator/(const Vector2F& other)
	//{
	//	this->x /= other.x;
	//	this->y /= other.y;
	//	return *this;
	//}
	float Vector2F::length() const
	{
		return std::sqrtf(this->x * this->x + this->y * this->y);
	}
	float Vector2F::length_squared() const
	{
		return this->x * this->x + this->y * this->y;
	}
	direction Vector2F::get_direction() const
	{
		if (are_equal(this->x, 0.0f) && are_equal(this->y, 0.0f))
		{
			return direction::NONE;
		}
		else if (are_equal(this->x, 0.0f))
		{
			if (this->y > 0.0f)
			{
				return direction::DOWN;
			}
			else
			{
				return direction::UP;
			}
		}
		else if (are_equal(this->y, 0.0f))
		{
			if (this->x > 0.0f)
			{
				return direction::RIGHT;
			}
			else
			{
				return direction::LEFT;
			}
		}
		else
		{
			if (this->x > 0.0f)
			{
				if (this->y > 0.0f)
				{
					return direction::DOWN_RIGHT;
				}
				else
				{
					return direction::UP_RIGHT;
				}
			}
			else
			{
				if (this->y > 0.0f)
				{
					return direction::DOWN_LEFT;
				}
				else
				{
					return direction::UP_LEFT;
				}
			}
		}
	}
	float Vector2F::dot(const Vector2F& other) const
	{
		return this->x * other.x + this->y * other.y;
	}
	Vector2F Vector2F::cross(const Vector2F& other) const
	{
		return Vector2F(this->x * other.y, this->y * other.x);
	}
	Vector2F Vector2F::normalized() const
	{
		float length = this->length();
		if (length == 0.0f)
		{
			return Vector2F::ZERO;
		}
		return Vector2F(this->x / length, this->y / length);
	}
	void Vector2F::normalize()
	{
		float length = this->length();
		if (length == 0.0f)
		{
			this->x = 0.0f;
			this->y = 0.0f;
			return;
		}
		this->x /= length;
		this->y /= length;
	}
	bool Vector2F::is_contained_within(const RectangleF& other) const
	{
		return other.intersects(*this);
	}
	void Vector2F::clamp(const Vector2F& min, const Vector2F& max)
	{
		this->x = std::min(std::max(this->x, min.x), max.x);
		this->y = std::min(std::max(this->y, min.y), max.y);
	}
	Vector2F Vector2F::clamped(const Vector2F& min, const Vector2F& max) const
	{
		return Vector2F(std::min(std::max(this->x, min.x), max.x),
			std::min(std::max(this->y, min.y), max.y));
	}
	float Vector2F::angle() const
	{
		return std::atan2(this->y, this->x);
	}
	float Vector2F::angle(const Vector2F& vec)
	{
		return std::atan2(vec.y, vec.x);
	}
	void Vector2F::rotate(float angle)
	{
		float cos_angle = std::cos(angle);
		float sin_angle = std::sin(angle);

		float rotated_x = this->x * cos_angle - this->y * sin_angle;
		float rotated_y = this->x * sin_angle + this->y * cos_angle;

		this->x = rotated_x;
		this->y = rotated_y;
	}
	void Vector2F::normal()
	{
		float temp = x;
		x = -y;
		y = temp;
	}
	void Vector2F::to_unit_vector()
	{
		float length = this->length();
		if (length == 0.0f)
		{
			this->x = 1.0f;
			this->y = 0.0f;
		}
		else
		{
			this->x /= length;
			this->y /= length;
		}
	}
	bool Vector2F::abs_x_greater_than_y() const
	{
		return std::abs(this->x) > std::abs(this->y);
	}
	Vector2F Vector2F::rotate_vector(const Vector2F& vec, float angle)
	{
		float cos_angle = std::cos(angle);
		float sin_angle = std::sin(angle);

		return Vector2F(vec.x * cos_angle - vec.y * sin_angle,
			vec.x * sin_angle + vec.y * cos_angle);
	}
	void Vector2F::rotate_vector_by_ref(Vector2F& vec, float angle)
	{
		float cos_angle = std::cos(angle);
		float sin_angle = std::sin(angle);

		vec.x = vec.x * cos_angle - vec.y * sin_angle;
		vec.y = vec.x * sin_angle + vec.y * cos_angle;
	}
	float Vector2F::angle_between(const Vector2F& a, const Vector2F& b)
	{
		return std::acos(Vector2F::dot(a, b) / (a.length() * b.length()));
	}
	Vector2F Vector2F::lerp(const Vector2F& a, const Vector2F& b, float t)
	{
		return Vector2F(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
	}
	Vector2F Vector2F::lerp(const Vector2F& a, const Vector2F& b, const Vector2F& t)
	{
		return Vector2F(a.x + (b.x - a.x) * t.x, a.y + (b.y - a.y) * t.y);
	}
	float Vector2F::distance(const Vector2F& a, const Vector2F& b)
	{
		return sqrtf((a.x - b.x) * (a.x - b.x) +
			(a.y - b.y) * (a.y - b.y));
	}
	float Vector2F::distance_squared(const Vector2F& a, const Vector2F& b)
	{
		return (a.x - b.x) * (a.x - b.x) +
			(a.y - b.y) * (a.y - b.y);
	}
	float Vector2F::dot(const Vector2F& a, const Vector2F& b)
	{
		return a.x * b.x + a.y * b.y;
	}
	Vector2F Vector2F::min_vec(const Vector2F& a, const Vector2F& b)
	{
		return Vector2F(std::min(a.x, b.x), std::min(a.y, b.y));
	}
	Vector2F Vector2F::max_vec(const Vector2F& a, const Vector2F& b)
	{
		return Vector2F(std::max(a.x, b.x), std::max(a.y, b.y));
	}
	Vector2F Vector2F::vec_from_angle_magnitude(float angle, float magnitude)
	{
		float x = magnitude * std::cos(angle);
		float y = magnitude * std::sin(angle);
		return Vector2F(x, y);
	}
	Vector2F Vector2F::unit_vec_from_angle(float angle)
	{
		return Vector2F(std::cos(angle), std::sin(angle));
	}
	Vector2F Vector2F::unit_vector(const Vector2F& vec)
	{
		float length = vec.length();
		if (length == 0.0f)
		{
			return Vector2F(1.0f, 0.0f);
		}
		else
		{
			return Vector2F(vec.x / length, vec.y / length);
		}
	}

	Vector2F Vector2F::normal(const Vector2F& vec)
	{
		return Vector2F(-vec.y, vec.x);
	}

	Vector2F Vector2F::direction_to_8_cardinal_direction(const Vector2F& direction)
	{
		float angle = direction.angle();
		if (angle >= -PI / 8.0f && angle < PI / 8.0f)
		{
			return Vector2F::DIRECTION_RIGHT;
		}
		else if (angle >= PI / 8.0f && angle < 3.0f * PI / 8.0f)
		{
			return Vector2F::DIRECTION_DOWN_RIGHT;
		}
		else if (angle >= 3.0f * PI / 8.0f && angle < 5.0f * PI / 8.0f)
		{
			return Vector2F::DIRECTION_DOWN;
		}
		else if (angle >= 5.0f * PI / 8.0f && angle < 7.0f * PI / 8.0f)
		{
			return Vector2F::DIRECTION_DOWN_LEFT;
		}
		else if (angle >= 7.0f * PI / 8.0f || angle < -7.0f * PI / 8.0f)
		{
			return Vector2F::DIRECTION_LEFT;
		}
		else if (angle >= -7.0f * PI / 8.0f && angle < -5.0f * PI / 8.0f)
		{
			return Vector2F::DIRECTION_UP_LEFT;
		}
		else if (angle >= -5.0f * PI / 8.0f && angle < -3.0f * PI / 8.0f)
		{
			return Vector2F::DIRECTION_UP;
		}
		else
		{
			return Vector2F::DIRECTION_UP_RIGHT;
		}
	}

	const Vector2F Vector2F::ZERO = { 0.0f, 0.0f };
	const Vector2F Vector2F::ONE = { 1.0f, 1.0f };
	const Vector2F Vector2F::DIRECTION_RIGHT = { 1.0f, 0.0f };
	const Vector2F Vector2F::DIRECTION_DOWN = { 0.0f, 1.0f };
	const Vector2F Vector2F::DIRECTION_LEFT = { -1.0f, 0.0f };
	const Vector2F Vector2F::DIRECTION_UP = { 0.0f, -1.0f };
	const Vector2F Vector2F::DIRECTION_UP_RIGHT = Vector2F::unit_vector(Vector2F::DIRECTION_UP + Vector2F::DIRECTION_RIGHT);
	const Vector2F Vector2F::DIRECTION_DOWN_RIGHT = Vector2F::unit_vector(Vector2F::DIRECTION_DOWN + Vector2F::DIRECTION_RIGHT);
	const Vector2F Vector2F::DIRECTION_DOWN_LEFT = Vector2F::unit_vector(Vector2F::DIRECTION_DOWN + Vector2F::DIRECTION_LEFT);
	const Vector2F Vector2F::DIRECTION_UP_LEFT = Vector2F::unit_vector(Vector2F::DIRECTION_UP + Vector2F::DIRECTION_LEFT);


	Vector2F mattmath::operator+ (const Vector2F& V1, const Vector2F& V2)
	{
		return Vector2F(V1.x + V2.x, V1.y + V2.y);
	}
	Vector2F mattmath::operator- (const Vector2F& V1, const Vector2F& V2)
	{
		return Vector2F(V1.x - V2.x, V1.y - V2.y);
	}
	Vector2F mattmath::operator* (const Vector2F& V1, const Vector2F& V2)
	{
		return Vector2F(V1.x * V2.x, V1.y * V2.y);
	}
	Vector2F mattmath::operator* (const Vector2F& V, float S)
	{
		return Vector2F(V.x * S, V.y * S);
	}
	Vector2F mattmath::operator/ (const Vector2F& V1, const Vector2F& V2)
	{
		return Vector2F(V1.x / V2.x, V1.y / V2.y);
	}
	Vector2F mattmath::operator/ (const Vector2F& V, float S)
	{
		return Vector2F(V.x / S, V.y / S);
	}
	Vector2F mattmath::operator* (float S, const Vector2F& V)
	{
		return Vector2F(V.x * S, V.y * S);
	}

#pragma endregion Vector2F

#pragma region RectangleI

	RectangleI::RectangleI(int x, int y, int width, int height)
	{
		this->x = x;
		this->y = y;
		this->width = width;
		this->height = height;
	}
	RectangleI::RectangleI(const Vector2I& position,
		const Vector2I& size)
	{
		this->x = position.x;
		this->y = position.y;
		this->width = size.x;
		this->height = size.y;
	}
	RectangleI::RectangleI(const Vector2F& position,const Vector2F& size)
	{
		this->x = static_cast<int>(position.x);
		this->y = static_cast<int>(position.y);
		this->width = static_cast<int>(size.x);
		this->height = static_cast<int>(size.y);
	}
	RectangleI::RectangleI(const SimpleMath::Vector2& position,
		const SimpleMath::Vector2& size)
	{
		this->x = static_cast<int>(position.x);
		this->y = static_cast<int>(position.y);
		this->width = static_cast<int>(size.x);
		this->height = static_cast<int>(size.y);
	}
	RectangleI::RectangleI(RectangleF rectangle)
	{
		this->x = static_cast<int>(rectangle.x);
		this->y = static_cast<int>(rectangle.y);
		this->width = static_cast<int>(rectangle.width);
		this->height = static_cast<int>(rectangle.height);
	}
	RectangleI::RectangleI(const SimpleMath::Rectangle& rectangle)
	{
		this->x = rectangle.x;
		this->y = rectangle.y;
		this->width = rectangle.width;
		this->height = rectangle.height;
	}
	RectangleI::RectangleI(const RECT& rectangle)
	{
		this->x = rectangle.left;
		this->y = rectangle.top;
		this->width = rectangle.right - rectangle.left;
		this->height = rectangle.bottom - rectangle.top;
	}

	int RectangleI::get_left() const
	{
		return this->x;
	}
	int RectangleI::get_top() const
	{
		return this->y;
	}
	int RectangleI::get_right() const
	{
		return this->x + this->width;
	}
	int RectangleI::get_bottom() const
	{
		return this->y + this->height;
	}
	Vector2I RectangleI::get_position() const
	{
		return Vector2I(this->x, this->y);
	}
	Vector2I RectangleI::get_size() const
	{
		return Vector2I(this->width, this->height);
	}
	Vector2I RectangleI::get_top_left() const
	{
		return Vector2I(this->x, this->y);
	}
	Vector2I RectangleI::get_bottom_right() const
	{
		return Vector2I(this->x + this->width, this->y + this->height);
	}
	DirectX::SimpleMath::Rectangle RectangleI::get_sm_rectangle() const
	{
		return SimpleMath::Rectangle(static_cast<long>(this->x),
			static_cast<long>(this->y),
			static_cast<long>(this->width),
			static_cast<long>(this->height));
	}
	RECT RectangleI::get_win_rect() const
	{
		RECT result = {
			static_cast<long>(this->x),
			static_cast<long>(this->y),
			static_cast<long>(this->x + this->width),
			static_cast<long>(this->y + this->height)
		};
		return result;
	}
	bool RectangleI::operator==(const RectangleI& other) const
	{
		return this->x == other.x &&
			this->y == other.y &&
			this->width == other.width &&
			this->height == other.height;
	}
	bool RectangleI::operator!=(const RectangleI& other) const
	{
		return !(*this == other);
	}
	bool RectangleI::contains(const Vector2I& point) const
	{
		return point.x >= this->x &&
			point.x <= this->x + this->width &&
			point.y >= this->y &&
			point.y <= this->y + this->height;

	}
	bool RectangleI::contains(const RectangleI& other) const
	{
		return other.x >= this->x &&
			other.x + other.width <= this->x + this->width &&
			other.y >= this->y &&
			other.y + other.height <= this->y + this->height;

	}
	void RectangleI::offset(int horizontal_amount, int vertical_amount)
	{
		this->x += horizontal_amount;
		this->y += vertical_amount;
	}
	void RectangleI::offset(const Vector2I& amount)
	{
		this->x += amount.x;
		this->y += amount.y;
	}
	void RectangleI::set_left(int left)
	{
		this->width += this->x - left;
		this->x = left;
	}
	void RectangleI::set_top(int top)
	{
		this->height += this->y - top;
		this->y = top;
	}
	void RectangleI::set_right(int right)
	{
		this->width = right - this->x;
	}
	void RectangleI::set_bottom(int bottom)
	{
		this->height = bottom - this->y;
	}
	void RectangleI::set_position(const Vector2I& position)
	{
		this->x = position.x;
		this->y = position.y;
	}
	void RectangleI::set_size(const Vector2I& size)
	{
		this->width = size.x;
		this->height = size.y;
	}
	void RectangleI::set_top_left(const Vector2I& top_left)
	{
		this->width += this->x - top_left.x;
		this->height += this->y - top_left.y;
		this->x = top_left.x;
		this->y = top_left.y;
	}
	void RectangleI::set_bottom_right(const Vector2I& bottom_right)
	{
		this->width = bottom_right.x - this->x;
		this->height = bottom_right.y - this->y;
	}
	void RectangleI::set_top_left_and_bottom_right(const Vector2I& top_left,
		const Vector2I& bottom_right)
	{
		this->width = bottom_right.x - top_left.x;
		this->height = bottom_right.y - top_left.y;
		this->x = top_left.x;
		this->y = top_left.y;
	}
	const RectangleI RectangleI::ZERO = { 0, 0, 0, 0 };

#pragma endregion RectangleI

#pragma region Vector4F

	Vector4F::Vector4F(float x, float y, float z, float w)
	{
		this->x = x;
		this->y = y;
		this->z = z;
		this->w = w;
	}
	Vector4F::Vector4F(const XMFLOAT4& vector)
	{
		this->x = vector.x;
		this->y = vector.y;
		this->z = vector.z;
		this->w = vector.w;
	}
	Vector4F& Vector4F::operator=(const XMFLOAT4& vector)
	{
		this->x = vector.x;
		this->y = vector.y;
		this->z = vector.z;
		this->w = vector.w;
		return *this;
	}
	bool Vector4F::operator==(const Vector4F& other) const
	{
		return this->x == other.x &&
			this->y == other.y &&
			this->z == other.z &&
			this->w == other.w;
	}
	bool Vector4F::operator!=(const Vector4F& other) const
	{
		return !(*this == other);
	}
	XMVECTOR Vector4F::get_xm_vector() const
	{
		XMVECTOR result = XMVectorSet(this->x, this->y, this->z, this->w);
		return result;
	}

#pragma endregion Vector4F

#pragma region Colour

	Colour::Colour(float r, float g, float b) :
		r(r), g(g), b(b), a(1.0f)
	{
		this->clamp_colours();
	}
	Colour::Colour(float r, float g, float b, float a) :
		r(r), g(g), b(b), a(a)
	{
		this->clamp_colours();
	}
	Colour::Colour(const DirectX::XMFLOAT4& vector) :
		r(vector.x), g(vector.y), b(vector.z), a(vector.w)
	{
		this->clamp_colours();
	}
	Colour::Colour(const DirectX::SimpleMath::Color& color) :
		r(color.R()), g(color.G()), b(color.B()), a(color.A())
	{
		this->clamp_colours();
	}
	Colour::Colour(int r, int g, int b, int a)
	{
		this->set_from_int_rgba(r, g, b, a);
	}
	Colour::Colour(const std::string& hex)
	{
		this->set_from_hex(hex);
	}
	Colour& Colour::operator=(const DirectX::XMFLOAT4& vector)
	{
		this->r = vector.x;
		this->g = vector.y;
		this->b = vector.z;
		this->a = vector.w;
		this->clamp_colours();
		return *this;
	}
	Colour& Colour::operator=(const DirectX::SimpleMath::Color& colour)
	{
		this->r = colour.R();
		this->g = colour.G();
		this->b = colour.B();
		this->a = colour.A();
		this->clamp_colours();
		return *this;
	}
	Colour& Colour::operator=(const mattmath::Vector4F& vector)
	{
		this->r = vector.x;
		this->g = vector.y;
		this->b = vector.z;
		this->a = vector.w;
		this->clamp_colours();
		return *this;
	}
	bool Colour::operator==(const Colour& other) const
	{
		return this->r == other.r &&
			this->g == other.g &&
			this->b == other.b &&
			this->a == other.a;
	}
	bool Colour::operator!=(const Colour& other) const
	{
		return !(*this == other);
	}
	Colour& Colour::operator+=(const Colour& other)
	{
		this->r += other.r;
		this->g += other.g;
		this->b += other.b;
		this->a += other.a;
		this->clamp_colours();
		return *this;
	}
	Colour& Colour::operator-=(const Colour& other)
	{
		this->r -= other.r;
		this->g -= other.g;
		this->b -= other.b;
		this->a -= other.a;
		this->clamp_colours();
		return *this;
	}
	Colour& Colour::operator*=(const Colour& other)
	{
		this->r *= other.r;
		this->g *= other.g;
		this->b *= other.b;
		this->a *= other.a;
		this->clamp_colours();
		return *this;
	}
	Colour& Colour::operator*=(float f)
	{
		this->r *= f;
		this->g *= f;
		this->b *= f;
		this->a *= f;
		this->clamp_colours();
		return *this;
	}
	Colour& Colour::operator/=(const Colour& other)
	{
		this->r /= other.r;
		this->g /= other.g;
		this->b /= other.b;
		this->a /= other.a;
		this->clamp_colours();
		return *this;
	}
	Colour& Colour::operator/=(float f)
	{
		this->r /= f;
		this->g /= f;
		this->b /= f;
		this->a /= f;
		this->clamp_colours();
		return *this;
	}
	float Colour::get_red() const
	{
		return this->r;
	}
	float Colour::get_green() const
	{
		return this->g;
	}
	float Colour::get_blue() const
	{
		return this->b;
	}
	float Colour::get_alpha() const
	{
		return this->a;
	}
	void Colour::set_red(float red)
	{
		this->r = red;
		this->clamp_colours();
	}
	void Colour::set_green(float green)
	{
		this->g = green;
		this->clamp_colours();
	}
	void Colour::set_blue(float blue)
	{
		this->b = blue;
		this->clamp_colours();
	}
	void Colour::set_alpha(float alpha)
	{
		this->a = alpha;
		this->clamp_colours();
	}
	void Colour::set(float red, float green, float blue, float alpha)
	{
		this->r = red;
		this->g = green;
		this->b = blue;
		this->a = alpha;
		this->clamp_colours();
	}
	void Colour::set_from_int_rgba(int red, int green, int blue, int alpha)
	{
		this->r = static_cast<float>(red) / 255.0f;
		this->g = static_cast<float>(green) / 255.0f;
		this->b = static_cast<float>(blue) / 255.0f;
		this->a = static_cast<float>(alpha) / 255.0f;
		this->clamp_colours();
	}
	void Colour::set_from_hex(const std::string& hex)
	{
		size_t hex_length = hex.length();
		if (hex_length == 6)
		{
			this->r = static_cast<float>(std::stoi(hex.substr(0, 2), nullptr, 16)) / 255.0f;
			this->g = static_cast<float>(std::stoi(hex.substr(2, 2), nullptr, 16)) / 255.0f;
			this->b = static_cast<float>(std::stoi(hex.substr(4, 2), nullptr, 16)) / 255.0f;
			this->a = 1.0f;
		}
		else if (hex_length == 8)
		{
			this->r = static_cast<float>(std::stoi(hex.substr(0, 2), nullptr, 16)) / 255.0f;
			this->g = static_cast<float>(std::stoi(hex.substr(2, 2), nullptr, 16)) / 255.0f;
			this->b = static_cast<float>(std::stoi(hex.substr(4, 2), nullptr, 16)) / 255.0f;
			this->a = static_cast<float>(std::stoi(hex.substr(6, 2), nullptr, 16)) / 255.0f;
		}
		else
		{
			this->r = 0.0f;
			this->g = 0.0f;
			this->b = 0.0f;
			this->a = 1.0f;
		}
		this->clamp_colours();
	}
	void Colour::saturate(float amount)
	{
		float grey = this->r * 0.3f + this->g * 0.59f + this->b * 0.11f;
		this->r = grey + amount * (this->r - grey);
		this->g = grey + amount * (this->g - grey);
		this->b = grey + amount * (this->b - grey);
		this->clamp_colours();
	}
	void Colour::desaturate(float amount)
	{
		float grey = this->r * 0.3f + this->g * 0.59f + this->b * 0.11f;
		this->r = grey + amount * (this->r - grey);
		this->g = grey + amount * (this->g - grey);
		this->b = grey + amount * (this->b - grey);
		this->clamp_colours();
	}
	void Colour::brighten(float amount)
	{
		this->r += amount;
		this->g += amount;
		this->b += amount;
		this->clamp_colours();
	}
	void Colour::darken(float amount)
	{
		this->r -= amount;
		this->g -= amount;
		this->b -= amount;
		this->clamp_colours();
	}
	void Colour::invert()
	{
		this->r = 1.0f - this->r;
		this->g = 1.0f - this->g;
		this->b = 1.0f - this->b;
		this->clamp_colours();
	}
	void Colour::make_opaque()
	{
		this->a = 1.0f;
	}
	void Colour::make_transparent()
	{
		this->a = 0.0f;
	}
	DirectX::SimpleMath::Color Colour::get_sm_colour() const
	{
		return DirectX::SimpleMath::Color(this->r, this->g, this->b, this->a);
	}
	DirectX::XMVECTOR Colour::get_xm_vector() const
	{
		return DirectX::XMVectorSet(this->r, this->g, this->b, this->a);
	}
	void Colour::clamp_colours()
	{
		this->r = clamp(this->r, 0.0f, 1.0f);
		this->g = clamp(this->g, 0.0f, 1.0f);
		this->b = clamp(this->b, 0.0f, 1.0f);
		this->a = clamp(this->a, 0.0f, 1.0f);
	}
	Colour mattmath::operator+ (const Colour& V1, const Colour& V2)
	{
		return Colour(V1.r + V2.r, V1.g + V2.g, V1.b + V2.b, V1.a + V2.a);
	}
	Colour mattmath::operator- (const Colour& V1, const Colour& V2)
	{
		return Colour(V1.r - V2.r, V1.g - V2.g, V1.b - V2.b, V1.a - V2.a);
	}
	Colour mattmath::operator* (const Colour& V1, const Colour& V2)
	{
		return Colour(V1.r * V2.r, V1.g * V2.g, V1.b * V2.b, V1.a * V2.a);
	}
	Colour mattmath::operator* (const Colour& V, float S)
	{
		return Colour(V.r * S, V.g * S, V.b * S, V.a * S);
	}
	Colour mattmath::operator/ (const Colour& V1, const Colour& V2)
	{
		return Colour(V1.r / V2.r, V1.g / V2.g, V1.b / V2.b, V1.a / V2.a);
	}
	Colour mattmath::operator/ (const Colour& V, float S)
	{
		return Colour(V.r / S, V.g / S, V.b / S, V.a / S);
	}
	Colour mattmath::operator* (float S, const Colour& V)
	{
		return Colour(V.r * S, V.g * S, V.b * S, V.a * S);
	}

#pragma endregion Colour

#pragma region MatrixF

	MatrixF::MatrixF(int rows, int columns)
	{
		this->rows_ = rows;
		this->columns_ = columns;
	}
	MatrixF::MatrixF(int rows, int columns, const std::vector<float>& elements)
	{
		this->rows_ = rows;
		this->columns_ = columns;
		if (rows * columns != elements.size())
		{
			throw std::invalid_argument(
				"Number of elements does not match the dimensions of the matrix");
		}
		this->elements_ = elements;
	}
	float MatrixF::get_element(int row, int column) const
	{
		// check if the row and column are valid
		if (!row_valid(row) || !column_valid(column))
		{
			throw std::invalid_argument("Row or column is not valid");
		}
		int index = calculate_index(row, column);
		return this->elements_[index];
	}
	void MatrixF::set_element(int row, int column, float value)
	{
		// check if the row and column are valid
		if (!row_valid(row) || !column_valid(column))
		{
			throw std::invalid_argument("Row or column is not valid");
		}
		int index = calculate_index(row, column);
		this->elements_[index] = value;
	}

	int MatrixF::get_rows() const
	{
		return this->rows_;
	}
	int MatrixF::get_columns() const
	{
		return this->columns_;
	}
	Vector2I MatrixF::get_dimensions() const
	{
		return Vector2I(this->columns_, this->rows_);
	}
	bool MatrixF::is_square() const
	{
		return this->rows_ == this->columns_;
	}
	bool MatrixF::is_identity() const
	{
		if (!this->is_square())
		{
			return false;
		}
		for (int i = 0; i < this->rows_; i++)
		{
			for (int j = 0; j < this->columns_; j++)
			{
				if (i == j)
				{
					if (this->get_element(i, j) != 1.0f)
					{
						return false;
					}
				}
				else
				{
					if (this->get_element(i, j) != 0.0f)
					{
						return false;
					}
				}
			}
		}
		return true;
	}
	bool MatrixF::is_symmetric() const
	{
		if (!this->is_square())
		{
			return false;
		}
		for (int i = 0; i < this->rows_; i++)
		{
			for (int j = 0; j < this->columns_; j++)
			{
				if (this->get_element(i, j) != this->get_element(j, i))
				{
					return false;
				}
			}
		}
		return true;
	}
	bool MatrixF::is_diagonal() const
	{
		if (!this->is_square())
		{
			return false;
		}
		for (int i = 0; i < this->rows_; i++)
		{
			for (int j = 0; j < this->columns_; j++)
			{
				if (i != j && this->get_element(i, j) != 0.0f)
				{
					return false;
				}
			}
		}
		return true;
	}
	bool MatrixF::is_upper_triangular() const
	{
		if (!this->is_square())
		{
			return false;
		}
		for (int i = 0; i < this->rows_; i++)
		{
			for (int j = 0; j < i; j++)
			{
				if (this->get_element(i, j) != 0.0f)
				{
					return false;
				}
			}
		}
		return true;
	}
	bool MatrixF::is_lower_triangular() const
	{
		if (!this->is_square())
		{
			return false;
		}
		for (int i = 0; i < this->rows_; i++)
		{
			for (int j = i + 1; j < this->columns_; j++)
			{
				if (this->get_element(i, j) != 0.0f)
				{
					return false;
				}
			}
		}
		return true;
	}
	bool MatrixF::is_invertible() const
	{
		if (!this->is_square())
		{
			return false;
		}
		return mattmath::determinant(*this) != 0.0f;
	}
	//bool MatrixF::is_row_echelon_form() const
	//{
	//
	//}
	//bool MatrixF::is_reduced_row_echelon_form() const
	//{
	//
	//}
	bool MatrixF::operator==(const MatrixF& other) const
	{
		Vector2I dimensions = this->get_dimensions();
		Vector2I other_dimensions = other.get_dimensions();
		if (dimensions != other_dimensions)
		{
			return false;
		}
		for (int i = 0; i < dimensions.x; i++)
		{
			for (int j = 0; j < dimensions.y; j++)
			{
				if (this->get_element(i, j) != other.get_element(i, j))
				{
					return false;
				}
			}
		}
		return true;
	}
	bool MatrixF::operator!=(const MatrixF& other) const
	{
		return !(*this == other);
	}
	MatrixF& MatrixF::operator+=(const MatrixF& other)
	{
		*this = mattmath::add(*this, other);
		return *this;
	}
	MatrixF& MatrixF::operator-=(const MatrixF& other)
	{
		*this = mattmath::subtract(*this, other);
		return *this;
	}
	MatrixF& MatrixF::operator*=(const MatrixF& other)
	{
		*this = mattmath::multiply(*this, other);
		return *this;
	}
	MatrixF& MatrixF::operator/=(const MatrixF& other)
	{
		*this = mattmath::divide(*this, other);
		return *this;
	}
	MatrixF& MatrixF::operator*=(float other)
	{
		*this = mattmath::multiply(*this, other);
		return *this;
	}
	MatrixF& MatrixF::operator/=(float other)
	{
		*this = mattmath::divide(*this, other);
		return *this;
	}
	bool MatrixF::row_valid(int row) const
	{
		return row >= 0 && row < this->rows_;
	}
	bool MatrixF::column_valid(int column) const
	{
		return column >= 0 && column < this->columns_;
	}
	int MatrixF::calculate_index(int row, int column) const
	{
		return row * this->columns_ + column;
	}
	MatrixF mattmath::operator+ (const MatrixF& a, const MatrixF& b)
	{
		return mattmath::add(a, b);
	}
	MatrixF mattmath::operator- (const MatrixF& a, const MatrixF& b)
	{
		return mattmath::subtract(a, b);
	}
	MatrixF mattmath::operator* (const MatrixF& a, const MatrixF& b)
	{
		return mattmath::multiply(a, b);
	}
	MatrixF mattmath::operator/ (const MatrixF& a, const MatrixF& b)
	{
		return mattmath::divide(a, b);
	}
	bool mattmath::equal_dimensions(const MatrixF& a, const MatrixF& b)
	{
		return a.get_dimensions() == b.get_dimensions();
	}
	MatrixF mattmath::add(const MatrixF& a, const MatrixF& b)
	{
		// Check if the dimensions are the same
		if (!equal_dimensions(a, b))
		{
			throw std::invalid_argument("Matrix dimensions are not the same");
		}
		int rows = a.get_rows();
		int columns = a.get_columns();
		MatrixF result(rows, columns);
		for (int i = 0; i < rows; i++)
		{
			for (int j = 0; j < columns; j++)
			{
				result.set_element(i, j, a.get_element(i, j) + b.get_element(i, j));
			}
		}
		return result;
	}
	MatrixF mattmath::subtract(const MatrixF& a, const MatrixF& b)
	{
		// Check if the dimensions are the same
		if (!equal_dimensions(a, b))
		{
			throw std::invalid_argument("Matrix dimensions are not the same");
		}
		int rows = a.get_rows();
		int columns = a.get_columns();
		MatrixF result(rows, columns);
		for (int i = 0; i < rows; i++)
		{
			for (int j = 0; j < columns; j++)
			{
				result.set_element(i, j, a.get_element(i, j) - b.get_element(i, j));
			}
		}
		return result;
	}
	MatrixF mattmath::multiply(const MatrixF& a, const MatrixF& b)
	{
		// check if the dimensions are correct
		if (a.get_columns() != b.get_rows())
		{
			throw std::invalid_argument("Matrix dimensions are not correct");
		}
		MatrixF result(a.get_rows(), b.get_columns());
		for (int i = 0; i < a.get_rows(); i++)
		{
			for (int j = 0; j < b.get_columns(); j++)
			{
				float sum = 0.0f;
				for (int k = 0; k < a.get_columns(); k++)
				{
					sum += a.get_element(i, k) * b.get_element(k, j);
				}
				result.set_element(i, j, sum);
			}
		}
		return result;
	}
	MatrixF mattmath::divide(const MatrixF& a, const MatrixF& b)
	{
		// check if the dimensions are correct
		if (a.get_columns() != b.get_rows())
		{
			throw std::invalid_argument("Matrix dimensions are not correct");
		}
		MatrixF result(a.get_rows(), b.get_columns());
		for (int i = 0; i < a.get_rows(); i++)
		{
			for (int j = 0; j < b.get_columns(); j++)
			{
				float sum = 0.0f;
				for (int k = 0; k < a.get_columns(); k++)
				{
					sum += a.get_element(i, k) / b.get_element(k, j);
				}
				result.set_element(i, j, sum);
			}
		}
		return result;
	}
	MatrixF mattmath::multiply(const MatrixF& a, float b)
	{
		int rows = a.get_rows();
		int columns = a.get_columns();
		MatrixF result(rows, columns);
		for (int i = 0; i < rows; i++)
		{
			for (int j = 0; j < columns; j++)
			{
				result.set_element(i, j, a.get_element(i, j) * b);
			}
		}
		return result;
	}
	MatrixF mattmath::divide(const MatrixF& a, float b)
	{
		int rows = a.get_rows();
		int columns = a.get_columns();
		MatrixF result(rows, columns);
		for (int i = 0; i < rows; i++)
		{
			for (int j = 0; j < columns; j++)
			{
				result.set_element(i, j, a.get_element(i, j) / b);
			}
		}
		return result;
	}
	MatrixF mattmath::gaussian_elimination(const MatrixF& matrix)
	{
		MatrixF result = matrix;
		int rows = result.get_rows();
		int columns = result.get_columns();
		int i = 0;
		int j = 0;
		while (i < rows && j < columns)
		{
			// Find the pivot
			int pivot_row = i;
			for (int k = i + 1; k < rows; k++)
			{
				if (std::abs(result.get_element(k, j)) > std::abs(result.get_element(pivot_row, j)))
				{
					pivot_row = k;
				}
			}
			if (result.get_element(pivot_row, j) != 0.0f)
			{
				// Swap the rows
				if (pivot_row != i)
				{
					for (int k = 0; k < columns; k++)
					{
						float temp = result.get_element(i, k);
						result.set_element(i, k, result.get_element(pivot_row, k));
						result.set_element(pivot_row, k, temp);
					}
				}
				// Make the pivot 1
				float pivot = result.get_element(i, j);
				for (int k = 0; k < columns; k++)
				{
					result.set_element(i, k, result.get_element(i, k) / pivot);
				}
				// Make the other elements in the column 0
				for (int k = 0; k < rows; k++)
				{
					if (k != i)
					{
						float factor = result.get_element(k, j);
						for (int l = 0; l < columns; l++)
						{
							result.set_element(k, l, result.get_element(k, l) - factor * result.get_element(i, l));
						}
					}
				}
				i++;
			}
			j++;
		}
		return result;
	}
	MatrixF mattmath::transpose(const MatrixF& matrix)
	{
		int rows = matrix.get_rows();
		int columns = matrix.get_columns();
		MatrixF result(columns, rows);
		for (int i = 0; i < rows; i++)
		{
			for (int j = 0; j < columns; j++)
			{
				result.set_element(j, i, matrix.get_element(i, j));
			}
		}
		return result;
	}
	//MatrixF mattmath::inverse(const MatrixF& matrix)
	//{
	//}
	MatrixF mattmath::identity(int size)
	{
		MatrixF result(size, size);
		for (int i = 0; i < size; i++)
		{
			result.set_element(i, i, 1.0f);
		}
		return result;
	}
	MatrixF mattmath::zero(int rows, int columns)
	{
		MatrixF result(rows, columns);
		for (int i = 0; i < rows; i++)
		{
			for (int j = 0; j < columns; j++)
			{
				result.set_element(i, j, 0.0f);
			}
		}
		return result;
	}
	float mattmath::determinant(const MatrixF& matrix)
	{
		if (!matrix.is_square())
		{
			throw std::invalid_argument("Matrix is not square");
		}
		int size = matrix.get_rows();
		if (size == 1)
		{
			return matrix.get_element(0, 0);
		}
		else if (size == 2)
		{
			return matrix.get_element(0, 0) * matrix.get_element(1, 1) -
				matrix.get_element(0, 1) * matrix.get_element(1, 0);
		}
		else
		{
			float result = 0.0f;
			for (int i = 0; i < size; i++)
			{
				MatrixF submatrix(size - 1, size - 1);
				for (int j = 1; j < size; j++)
				{
					for (int k = 0; k < size; k++)
					{
						if (k < i)
						{
							submatrix.set_element(j - 1, k, matrix.get_element(j, k));
						}
						else if (k > i)
						{
							submatrix.set_element(j - 1, k - 1, matrix.get_element(j, k));
						}
					}
				}
				result += matrix.get_element(0, i) * determinant(submatrix) * ((i % 2 == 0) ? 1 : -1);
			}
			return result;
		}
	}
	//std::vector<MatrixF> mattmath::eigenvectors(const MatrixF& matrix)
	//{
	//}
	//std::vector<float> mattmath::eigenvalues(const MatrixF& matrix)
	//{
	//}

#pragma endregion MatrixF

#pragma region Matrix3x3F

	Matrix3x3F::Matrix3x3F(const std::vector<float>& elements) :
		MatrixF(3, 3, elements)
	{
		if (elements.size() != 9)
		{
			throw std::invalid_argument("Elements vector must have 9 elements");
		}
	}
	Matrix3x3F::Matrix3x3F(float e11, float e12, float e13,
		float e21, float e22, float e23,
		float e31, float e32, float e33) :
		MatrixF(3, 3)
	{
		this->set_element(0, 0, e11);
		this->set_element(0, 1, e12);
		this->set_element(0, 2, e13);
		this->set_element(1, 0, e21);
		this->set_element(1, 1, e22);
		this->set_element(1, 2, e23);
		this->set_element(2, 0, e31);
		this->set_element(2, 1, e32);
		this->set_element(2, 2, e33);
	}
	Matrix3x3F Matrix3x3F::rotation(float angle)
	{
		float cos_angle = std::cos(angle);
		float sin_angle = std::sin(angle);
		return Matrix3x3F(cos_angle, -sin_angle, 0.0f,
			sin_angle, cos_angle, 0.0f,
			0.0f, 0.0f, 1.0f);
	}
	Matrix3x3F Matrix3x3F::scale(float x, float y)
	{
		return Matrix3x3F(x, 0.0f, 0.0f,
			0.0f, y, 0.0f,
			0.0f, 0.0f, 1.0f);
	}
	Matrix3x3F Matrix3x3F::scale(float scale)
	{
		return Matrix3x3F(scale, 0.0f, 0.0f,
			0.0f, scale, 0.0f,
			0.0f, 0.0f, 1.0f);
	}
	Matrix3x3F Matrix3x3F::scale(const Vector2F& scale)
	{
		return Matrix3x3F(scale.x, 0.0f, 0.0f,
			0.0f, scale.y, 0.0f,
			0.0f, 0.0f, 1.0f);

	}
	Matrix3x3F Matrix3x3F::translation(float x, float y)
	{
		return Matrix3x3F(1.0f, 0.0f, x,
			0.0f, 1.0f, y,
			0.0f, 0.0f, 1.0f);
	}
	Matrix3x3F Matrix3x3F::translation(const Vector2F& translation)
	{
		return Matrix3x3F(1.0f, 0.0f, translation.x,
			0.0f, 1.0f, translation.y,
			0.0f, 0.0f, 1.0f);
	}

#pragma endregion Matrix3x3F

#pragma region Vector3F

	Vector3F::Vector3F(float x, float y, float z) :
		x(x), y(y), z(z)
	{
	}
	bool Vector3F::operator==(const Vector3F& other) const
	{
		return this->x == other.x &&
			this->y == other.y &&
			this->z == other.z;
	}
	bool Vector3F::operator!=(const Vector3F& other) const
	{
		return !(*this == other);
	}
	Vector3F& Vector3F::operator+=(const Vector3F& other)
	{
		this->x += other.x;
		this->y += other.y;
		this->z += other.z;
		return *this;
	}
	Vector3F& Vector3F::operator-=(const Vector3F& other)
	{
		this->x -= other.x;
		this->y -= other.y;
		this->z -= other.z;
		return *this;
	}
	Vector3F& Vector3F::operator*=(const Vector3F& other)
	{
		this->x *= other.x;
		this->y *= other.y;
		this->z *= other.z;
		return *this;
	}
	Vector3F& Vector3F::operator/=(const Vector3F& other)
	{
		this->x /= other.x;
		this->y /= other.y;
		this->z /= other.z;
		return *this;
	}
	Vector3F& Vector3F::operator*=(float other)
	{
		this->x *= other;
		this->y *= other;
		this->z *= other;
		return *this;
	}
	Vector3F& Vector3F::operator/=(float other)
	{
		this->x /= other;
		this->y /= other;
		this->z /= other;
		return *this;
	}
	Vector3F mattmath::operator+ (const Vector3F& V1, const Vector3F& V2)
	{
		return Vector3F(V1.x + V2.x, V1.y + V2.y, V1.z + V2.z);
	}
	Vector3F mattmath::operator- (const Vector3F& V1, const Vector3F& V2)
	{
		return Vector3F(V1.x - V2.x, V1.y - V2.y, V1.z - V2.z);
	}
	Vector3F mattmath::operator* (const Vector3F& V1, const Vector3F& V2)
	{
		return Vector3F(V1.x * V2.x, V1.y * V2.y, V1.z * V2.z);
	}
	Vector3F mattmath::operator* (const Vector3F& V, float S)
	{
		return Vector3F(V.x * S, V.y * S, V.z * S);
	}
	Vector3F mattmath::operator/ (const Vector3F& V1, const Vector3F& V2)
	{
		return Vector3F(V1.x / V2.x, V1.y / V2.y, V1.z / V2.z);
	}
	Vector3F mattmath::operator/ (const Vector3F& V, float S)
	{
		return Vector3F(V.x / S, V.y / S, V.z / S);
	}
	Vector3F mattmath::operator* (float S, const Vector3F& V)
	{
		return Vector3F(V.x * S, V.y * S, V.z * S);
	}
	//Vector3F Vector3F::operator+(const Vector3F& other) const
	//{
	//	return Vector3F(this->x + other.x, this->y + other.y, this->z + other.z);
	//}
	//Vector3F Vector3F::operator-(const Vector3F& other) const
	//{
	//	return Vector3F(this->x - other.x, this->y - other.y, this->z - other.z);
	//}

#pragma endregion Vector3F

#pragma region Viewport

	Viewport::Viewport(float x, float y, float width, float height,
		float minDepth, float maxDepth) :
		x(x), y(y), width(width), height(height),
		minDepth(minDepth), maxDepth(maxDepth)
	{

	}
	Viewport::Viewport(const SimpleMath::Viewport& viewport)
	{
		this->x = viewport.x;
		this->y = viewport.y;
		this->width = viewport.width;
		this->height = viewport.height;
		this->minDepth = viewport.minDepth;
		this->maxDepth = viewport.maxDepth;
	}
	Viewport::Viewport(const RectangleF& rectangle,
		float minDepth, float maxDepth)
	{
		this->x = rectangle.x;
		this->y = rectangle.y;
		this->width = rectangle.width;
		this->height = rectangle.height;
		this->minDepth = minDepth;
		this->maxDepth = maxDepth;
	}
	Viewport::Viewport(const RectangleI& rectangle,
		float minDepth, float maxDepth)
	{
		this->x = static_cast<float>(rectangle.x);
		this->y = static_cast<float>(rectangle.y);
		this->width = static_cast<float>(rectangle.width);
		this->height = static_cast<float>(rectangle.height);
		this->minDepth = minDepth;
		this->maxDepth = maxDepth;
	}
	Viewport::Viewport(const D3D11_VIEWPORT& viewport)
	{
		this->x = viewport.TopLeftX;
		this->y = viewport.TopLeftY;
		this->width = viewport.Width;
		this->height = viewport.Height;
		this->minDepth = viewport.MinDepth;
		this->maxDepth = viewport.MaxDepth;
	}
	SimpleMath::Viewport Viewport::get_sm_viewport() const
	{
		return SimpleMath::Viewport(this->x, this->y, this->width, this->height,
			this->minDepth, this->maxDepth);
	}
	D3D11_VIEWPORT Viewport::get_d3d_viewport() const
	{
		D3D11_VIEWPORT result = {
			this->x,
			this->y,
			this->width,
			this->height,
			this->minDepth,
			this->maxDepth
		};
		return result;
	}
	const D3D11_VIEWPORT* Viewport::get_d3d_viewport_ptr() const
	{
		return reinterpret_cast<const D3D11_VIEWPORT*>(this);
	}
	RectangleF Viewport::get_rectangle() const
	{
		return RectangleF(this->x, this->y, this->width, this->height);
	}
	//RectangleF Viewport::get_rectangle(float minDepth, float maxDepth) const
	//{
	//	return RectangleF(this->x, this->y, this->width, this->height);
	//}
	Vector2F Viewport::get_position() const
	{
		return Vector2F(this->x, this->y);
	}
	Vector2F Viewport::get_size() const
	{
		return Vector2F(this->width, this->height);
	}
	//Viewport& Viewport::operator=(const Viewport& viewport)
	//{
	//	this->x = viewport.x;
	//	this->y = viewport.y;
	//	this->width = viewport.width;
	//	this->height = viewport.height;
	//	this->minDepth = viewport.minDepth;
	//	this->maxDepth = viewport.maxDepth;
	//	return *this;
	//}
	Viewport& Viewport::operator=(const SimpleMath::Viewport& viewport)
	{
		this->x = viewport.x;
		this->y = viewport.y;
		this->width = viewport.width;
		this->height = viewport.height;
		this->minDepth = viewport.minDepth;
		this->maxDepth = viewport.maxDepth;
		return *this;
	}
	Viewport& Viewport::operator=(const D3D11_VIEWPORT& viewport)
	{
		this->x = viewport.TopLeftX;
		this->y = viewport.TopLeftY;
		this->width = viewport.Width;
		this->height = viewport.Height;
		this->minDepth = viewport.MinDepth;
		this->maxDepth = viewport.MaxDepth;
		return *this;
	}
	Viewport& Viewport::operator=(const mattmath::RectangleF& rectangle)
	{
		this->x = rectangle.x;
		this->y = rectangle.y;
		this->width = rectangle.width;
		this->height = rectangle.height;
		return *this;
	}
	Viewport& Viewport::operator=(const mattmath::RectangleI& rectangle)
	{
		this->x = static_cast<float>(rectangle.x);
		this->y = static_cast<float>(rectangle.y);
		this->width = static_cast<float>(rectangle.width);
		this->height = static_cast<float>(rectangle.height);
		return *this;
	}
	Viewport& Viewport::operator=(const RECT& rect)
	{
		this->x = static_cast<float>(rect.left);
		this->y = static_cast<float>(rect.top);
		this->width = static_cast<float>(rect.right - rect.left);
		this->height = static_cast<float>(rect.bottom - rect.top);
		return *this;
	}
	bool Viewport::operator==(const Viewport& other) const
	{
		return this->x == other.x &&
			this->y == other.y &&
			this->width == other.width &&
			this->height == other.height &&
			this->minDepth == other.minDepth &&
			this->maxDepth == other.maxDepth;
	}
	bool Viewport::operator!=(const Viewport& other) const
	{
		return !(*this == other);
	}

#pragma endregion Viewport

#pragma region Circle

	Circle::Circle(const Vector2F& center, float radius) :
		center(center), radius(radius)
	{
	}
	Circle::Circle(const DirectX::SimpleMath::Vector2& center, float radius) :
		center(center), radius(radius)
	{
	}
	Circle::Circle(float x, float y, float radius)
	{
		this->center = Vector2F(x, y);
		this->radius = radius;
	}
	RectangleF Circle::get_bounding_box() const
	{
		return mattmath::RectangleF(this->center.x - this->radius,
					this->center.y - this->radius,
					this->radius * 2.0f, this->radius * 2.0f);

	}
	shape_type Circle::get_shape_type() const
	{
		return shape_type::CIRCLE;
	}

	void Circle::offset(const Vector2F& offset)
	{
		this->center += offset;
	}

	std::unique_ptr<Shape> Circle::clone() const
	{
		return std::make_unique<Circle>(*this);
	}

	std::vector<Segment> Circle::get_edges() const
	{
		// Circles have no edges
		return std::vector<Segment>();
	}

	void Circle::inflate(float amount)
	{
		this->radius += amount;
	}

	bool Circle::operator==(const Circle& other) const
	{
		return this->center == other.center &&
			this->radius == other.radius;
	}
	bool Circle::operator!=(const Circle& other) const
	{
		return !(*this == other);
	}
	//bool Circle::contains(const Vector2F& point) const
	//{
	//	return Vector2F::distance(this->center, point) <= this->radius;
	//}
	bool Circle::intersects(const RectangleF& other) const
	{
		return rectangle_circle_intersect(other, *this);
	}
	bool Circle::intersects(const Circle& other) const
	{
		return circles_intersect(*this, other);
	}
	bool Circle::intersects(const Triangle& other) const
	{
		return circle_triangle_intersect(*this, other);
	}
	bool Circle::intersects(const Quad& other) const
	{
		return circle_quad_intersect(*this, other);
	}
	bool Circle::intersects(const Segment& other) const
	{
		return circle_segment_intersect(*this, other);
	}
	bool Circle::intersects(const Point2F& other) const
	{
		return circle_point_intersect(*this, other);
	}
	bool Circle::intersects(const RectangleRotated& rect_rotated) const
	{
		return circle_rectangle_rotated_intersect(*this, rect_rotated);
	}
	bool Circle::contains(const Point2F& point) const
	{
		return this->intersects(point);
	}
	Point2F Circle::get_center() const
	{
		return this->center;
	}

#pragma endregion Circle

#pragma region Triangle

	Triangle::Triangle(const Vector2F& point0,
		const Vector2F& point1,
		const Vector2F& point2)
	{
		this->points[0] = point0;
		this->points[1] = point1;
		this->points[2] = point2;
	}
	Triangle::Triangle(const DirectX::SimpleMath::Vector2& point0,
		const DirectX::SimpleMath::Vector2& point1,
		const DirectX::SimpleMath::Vector2& point2)
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
	RectangleF Triangle::get_bounding_box() const
	{
		float x1 = std::min(std::min(this->points[0].x, this->points[1].x),
			this->points[2].x);

		float x2 = std::max(std::max(this->points[0].x, this->points[1].x), 
			this->points[2].x);

		float y1 = std::min(std::min(this->points[0].y, this->points[1].y), 
			this->points[2].y);

		float y2 = std::max(std::max(this->points[0].y, this->points[1].y), 
			this->points[2].y);

		return RectangleF(x1, y1, x2 - x1, y2 - y1);
	}
	shape_type Triangle::get_shape_type() const
	{
		return shape_type::TRIANGLE;
	}
	void Triangle::offset(const Vector2F& offset)
	{
		this->points[0] += offset;
		this->points[1] += offset;
		this->points[2] += offset;
	}
	std::unique_ptr<Shape> Triangle::clone() const
	{
		return std::make_unique<Triangle>(*this);
	}
	void Triangle::inflate(float amount)
	{
		Vector2F center = this->get_center();
		Vector2F edge0 = this->points[0] - center;
		Vector2F edge1 = this->points[1] - center;
		Vector2F edge2 = this->points[2] - center;

		edge0.normalize();
		edge1.normalize();
		edge2.normalize();

		this->points[0] = center + edge0 * (this->points[0] - center).length() + edge0 * amount;
		this->points[1] = center + edge1 * (this->points[1] - center).length() + edge1 * amount;
		this->points[2] = center + edge2 * (this->points[2] - center).length() + edge2 * amount;

	}
	const Vector2F& Triangle::get_point_0() const
	{
		return this->points[0];
	}
	const Vector2F& Triangle::get_point_1() const
	{
		return this->points[1];
	}
	const Vector2F& Triangle::get_point_2() const
	{
		return this->points[2];
	}
	std::vector<Vector2F> Triangle::get_points() const
	{
		return { this->points[0], this->points[1], this->points[2] };
	}
	Segment Triangle::get_edge_0() const
	{
		return Segment(this->points[0], this->points[1]);
	}
	Segment Triangle::get_edge_1() const
	{
		return Segment(this->points[1], this->points[2]);
	}
	Segment Triangle::get_edge_2() const
	{
		return Segment(this->points[2], this->points[0]);
	}
	std::vector<Segment> Triangle::get_edges() const
	{
		std::vector<Segment> segments =
		{
			this->get_edge_0(),
			this->get_edge_1(),
			this->get_edge_2()
		};
		return segments;
	}
	float Triangle::get_angle_0() const
	{
		return Vector2F::angle_between(this->get_edge_0().get_direction(),
			this->get_edge_2().get_direction());

	}
	float Triangle::get_angle_1() const
	{
		return Vector2F::angle_between(this->get_edge_1().get_direction(),
			this->get_edge_0().get_direction());
	}
	float Triangle::get_angle_2() const
	{
		return Vector2F::angle_between(this->get_edge_2().get_direction(),
			this->get_edge_1().get_direction());
	}
	std::vector<float> Triangle::get_angles() const
	{
		std::vector<float> angles =
		{
			this->get_angle_0(),
			this->get_angle_1(),
			this->get_angle_2()
		};
		return angles;
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

	bool Triangle::intersects(const RectangleF& other) const
	{
		return rectangle_triangle_intersect(other, *this);
	}

	bool Triangle::intersects(const Circle& other) const
	{
		return circle_triangle_intersect(other, *this);
	}

	bool Triangle::intersects(const Triangle& other) const
	{
		return triangles_intersect(*this, other);
	}

	bool Triangle::intersects(const Quad& other) const
	{
		return triangle_quad_intersect(*this, other);
	}

	bool Triangle::intersects(const Segment& other) const
	{
		return triangle_segment_intersect(*this, other);
	}

	bool Triangle::intersects(const Point2F& other) const
	{
		return triangle_point_intersect(*this, other);
	}

	bool Triangle::intersects(const RectangleRotated& rect_rotated) const
	{
		return triangle_rectangle_rotated_intersect(*this, rect_rotated);
	}

	bool Triangle::contains(const Point2F& point) const
	{
		return this->intersects(point);
	}

	Point2F Triangle::get_center() const
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

#pragma endregion Triangle

#pragma region TriangleRightAxisAligned

	TriangleRightAxisAligned::TriangleRightAxisAligned(
		const Vector2F& top,
		const Vector2F& left, const Vector2F& right) :
		Triangle(top, left, right)
	{

	}

	TriangleRightAxisAligned::TriangleRightAxisAligned(
		const DirectX::SimpleMath::Vector2& top,
		const DirectX::SimpleMath::Vector2& left,
		const DirectX::SimpleMath::Vector2& right) :
		Triangle(top, left, right)
	{

	}

	TriangleRightAxisAligned::TriangleRightAxisAligned(
		float x0, float y0, float x1, float y1, float x2, float y2) :
		Triangle(x0, y0, x1, y1, x2, y2)
	{

	}

	Segment TriangleRightAxisAligned::get_hypotenuse() const
	{
		int hypotenuse = this->find_hypotenuse(*this);
		if (hypotenuse == -1)
		{
			throw std::invalid_argument("Triangle is not a right triangle");
		}

		return this->get_edges()[hypotenuse];
	}
	float TriangleRightAxisAligned::get_hypotenuse_gradient() const
	{
		int hypotenuse = this->find_hypotenuse(*this);
		if (hypotenuse == -1)
		{
			throw std::invalid_argument("Triangle is not a right triangle");
		}

		return this->calculate_gradient(hypotenuse);
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
		if (are_equal(tri.get_angle_0(), PI_OVER_2))
		{
			return 0;
		}
		else if (are_equal(tri.get_angle_1(), PI_OVER_2))
		{
			return 1;
		}
		else if (are_equal(tri.get_angle_2(), PI_OVER_2))
		{
			return 2;
		}
		else
		{
			return -1;
		}
	}
	float TriangleRightAxisAligned::calculate_gradient(int edge) const
	{
		if (edge < 0 || edge > 2)
		{
			throw std::invalid_argument("Edge must be between 0 and 2");
		}

		return (this->points[(edge + 1) % 3].y - this->points[edge].y) /
			(this->points[(edge + 1) % 3].x - this->points[edge].x);
	}

#pragma endregion TriangleRightAxisAligned

#pragma region Quad

	Quad::Quad(const Vector2F& point0,
		const Vector2F& point1,
		const Vector2F& point2,
		const Vector2F& point3)
	{
		this->points[0] = point0;
		this->points[1] = point1;
		this->points[2] = point2;
		this->points[3] = point3;

		if (!this->is_valid())
		{
			throw std::invalid_argument("Quad is not valid");
		}
	}

	Quad::Quad(const std::vector<Point2F>& points)
	{
		if (points.size() != 4)
		{
			throw std::invalid_argument("Quad must have 4 points");
		}

		this->points[0] = points[0];
		this->points[1] = points[1];
		this->points[2] = points[2];
		this->points[3] = points[3];

		if (!this->is_valid())
		{
			throw std::invalid_argument("Quad is not valid");
		}
	}

	Quad::Quad(const RectangleF& rectangle)
	{
		this->points[0] = Vector2F(rectangle.x, rectangle.y);
		this->points[1] = Vector2F(rectangle.x + rectangle.width, rectangle.y);
		this->points[2] = Vector2F(rectangle.x + rectangle.width, rectangle.y + rectangle.height);
		this->points[3] = Vector2F(rectangle.x, rectangle.y + rectangle.height);

		if (!this->is_valid())
		{
			throw std::invalid_argument("Quad is not valid");
		}
	}

	Quad::Quad(const RectangleRotated& rectangle)
	{
		Quad q = rectangle.get_quad();

		this->points[0] = q.get_point_0();
		this->points[1] = q.get_point_1();
		this->points[2] = q.get_point_2();
		this->points[3] = q.get_point_3();

		if (!this->is_valid())
		{
			throw std::invalid_argument("Quad is not valid");
		}
	}

	Quad::Quad(const DirectX::SimpleMath::Vector2& point0,
		const DirectX::SimpleMath::Vector2& point1,
		const DirectX::SimpleMath::Vector2& point2,
		const DirectX::SimpleMath::Vector2& point3)
	{
		this->points[0] = point0;
		this->points[1] = point1;
		this->points[2] = point2;
		this->points[3] = point3;

		if (!this->is_valid())
		{
			throw std::invalid_argument("Quad is not valid");
		}
	}

	RectangleF Quad::get_bounding_box() const
	{
		float x1 = std::min(std::min(std::min(this->points[0].x, this->points[1].x),
			this->points[2].x), this->points[3].x);

		float x2 = std::max(std::max(std::max(this->points[0].x, this->points[1].x),
			this->points[2].x), this->points[3].x);

		float y1 = std::min(std::min(std::min(this->points[0].y, this->points[1].y),
			this->points[2].y), this->points[3].y);

		float y2 = std::max(std::max(std::max(this->points[0].y, this->points[1].y),
			this->points[2].y), this->points[3].y);

		return RectangleF(x1, y1, x2 - x1, y2 - y1);
	}

	shape_type Quad::get_shape_type() const
	{
		return shape_type::QUAD;
	}

	void Quad::offset(const Vector2F& offset)
	{
		this->points[0] += offset;
		this->points[1] += offset;
		this->points[2] += offset;
		this->points[3] += offset;
	}

	std::unique_ptr<Shape> Quad::clone() const
	{
		return std::make_unique<Quad>(*this);
	}

	void Quad::inflate(float amount)
	{
		Vector2F center = this->get_center();
		Vector2F edge0 = this->points[0] - center;
		Vector2F edge1 = this->points[1] - center;
		Vector2F edge2 = this->points[2] - center;
		Vector2F edge3 = this->points[3] - center;

		edge0.normalize();
		edge1.normalize();
		edge2.normalize();
		edge3.normalize();

		this->points[0] = center + edge0 * (this->points[0] - center).length() + edge0 * amount;
		this->points[1] = center + edge1 * (this->points[1] - center).length() + edge1 * amount;
		this->points[2] = center + edge2 * (this->points[2] - center).length() + edge2 * amount;
		this->points[3] = center + edge3 * (this->points[3] - center).length() + edge3 * amount;
	}

	bool Quad::is_valid() const
	{
		// check if the edges intersect
		std::vector<Segment> edges = this->get_edges();

		for (int i = 0; i < 4; i++)
		{
			for (int j = i + 1; j < 4; j++)
			{
				if (edges[i].intersects(edges[j]))
				{
					return false;
				}
			}
		}

		return true;
	}

	const Point2F& Quad::get_point_0() const
	{
		return this->points[0];
	}

	const Point2F& Quad::get_point_1() const
	{
		return this->points[1];
	}

	const Point2F& Quad::get_point_2() const
	{
		return this->points[2];
	}

	const Point2F& Quad::get_point_3() const
	{
		return this->points[3];
	}

	std::vector<Point2F> Quad::get_points() const
	{
		return
		{
			this->points[0],
			this->points[1],
			this->points[2],
			this->points[3]
		};
	}

	void Quad::set_point_0(const Point2F& point)
	{
		this->points[0] = point;

		if (!this->is_valid())
		{
			throw std::invalid_argument("Quad is not valid");
		}
	}

	void Quad::set_point_1(const Point2F& point)
	{
		this->points[1] = point;

		if (!this->is_valid())
		{
			throw std::invalid_argument("Quad is not valid");
		}
	}

	void Quad::set_point_2(const Point2F& point)
	{
		this->points[2] = point;

		if (!this->is_valid())
		{
			throw std::invalid_argument("Quad is not valid");
		}
	}

	void Quad::set_point_3(const Point2F& point)
	{
		this->points[3] = point;

		if (!this->is_valid())
		{
			throw std::invalid_argument("Quad is not valid");
		}
	}

	Segment Quad::get_edge_0() const
	{
		return Segment(this->points[0], this->points[1]);
	}

	Segment Quad::get_edge_1() const
	{
		return Segment(this->points[1], this->points[2]);
	}

	Segment Quad::get_edge_2() const
	{
		return Segment(this->points[2], this->points[3]);
	}

	Segment Quad::get_edge_3() const
	{
		return Segment(this->points[3], this->points[0]);
	}

	std::vector<Segment> Quad::get_edges() const
	{
		std::vector<Segment> edges =
		{
			this->get_edge_0(),
			this->get_edge_1(),
			this->get_edge_2(),
			this->get_edge_3()
		};
		return edges;
	}

	Triangle Quad::get_triangle_0() const
	{
		return Triangle(this->points[0], this->points[1], this->points[2]);
	}

	Triangle Quad::get_triangle_1() const
	{
		return Triangle(this->points[0], this->points[2], this->points[3]);
	}

	std::vector<Triangle> Quad::get_triangles() const
	{
		std::vector<Triangle> triangles =
		{
			this->get_triangle_0(),
			this->get_triangle_1()
		};
		return triangles;
	}

	bool Quad::operator==(const Quad& other) const
	{
		return this->points[0] == other.points[0] &&
			this->points[1] == other.points[1] &&
			this->points[2] == other.points[2] &&
			this->points[3] == other.points[3];
	}

	bool Quad::operator!=(const Quad& other) const
	{
		return !(*this == other);
	}

	bool Quad::intersects(const RectangleF& other) const
	{
		return rectangle_quad_intersect(other, *this);
	}

	bool Quad::intersects(const Circle& other) const
	{
		return circle_quad_intersect(other, *this);
	}

	bool Quad::intersects(const Triangle& other) const
	{
		return triangle_quad_intersect(other, *this);
	}

	bool Quad::intersects(const Quad& other) const
	{
		return quads_intersect(*this, other);
	}

	bool Quad::intersects(const Segment& other) const
	{
		return quad_segment_intersect(*this, other);
	}

	bool Quad::intersects(const Point2F& other) const
	{
		return quad_point_intersect(*this, other);
	}

	bool Quad::intersects(const RectangleRotated& rect_rotated) const
	{
		return quad_rectangle_rotated_intersect(*this, rect_rotated);
	}

	bool Quad::contains(const Point2F& point) const
	{
		return this->intersects(point);
	}

	Point2F Quad::get_center() const
	{
		return (this->points[0] + this->points[1] + this->points[2] + this->points[3]) / 4.0f;
	}

#pragma endregion Quad

#pragma region Segment

	Segment::Segment(const Point2F& point_0, const Point2F& point_1) :
		point_0(point_0), point_1(point_1)
	{
	}
	Segment::Segment(float x0, float y0, float x1, float y1) :
		point_0(x0, y0), point_1(x1, y1)
	{
	}
	bool Segment::operator==(const Segment& other) const
	{
		return this->point_0 == other.point_0 &&
			this->point_1 == other.point_1;
	}
	bool Segment::operator!=(const Segment& other) const
	{
		return !(*this == other);
	}
	bool Segment::intersects(const Segment& other) const
	{
		return segments_intersect(*this, other);
	}
	bool Segment::intersects(const RectangleF& other) const
	{
		return other.intersects(*this);

	}
	Vector2F Segment::get_direction() const
	{
		return this->point_1 - this->point_0;
	}
	float Segment::get_length() const
	{
		return this->get_direction().length();
	}
	Point2F Segment::get_center() const
	{
		return (this->point_0 + this->point_1) / 2.0f;
	}
	//bool Segment::intersects(const RectangleF& other) const
	//{
	//
	//}
	//bool Segment::intersects(const Circle& other) const
	//{
	//
	//}
	//bool Segment::intersects(const Triangle& other) const
	//{
	//
	//}

#pragma endregion Segment

#pragma region Camera

	Camera::Camera(const Vector2F& translation, float scale) :
		translation(translation), scale(scale)
	{
	}
	Camera::Camera(float x, float y, float scale) :
		translation(x, y), scale(scale)
	{
	}
	Camera::Camera(const Viewport& viewport, float scale)
	{
		this->translation.x = viewport.x;
		this->translation.y = viewport.y;
		this->scale = scale;
	}

	bool Camera::operator==(const Camera& other) const
	{
		return this->translation == other.translation &&
			this->scale == other.scale;
	}
	bool Camera::operator!=(const Camera& other) const
	{
		return !(*this == other);
	}
	RectangleF Camera::calculate_view_rectangle(
		const RectangleF& world_rectangle) const
	{
		return RectangleF((world_rectangle.x - this->translation.x) * this->scale,
			(world_rectangle.y - this->translation.y) * this->scale,
			world_rectangle.width * this->scale,
			world_rectangle.height * this->scale);
	}
	void Camera::calculate_view_rectangle(RectangleF& rectangle) const
	{
		rectangle.x = (rectangle.x - this->translation.x) * this->scale;
		rectangle.y = (rectangle.y - this->translation.y) * this->scale;
		rectangle.width *= this->scale;
		rectangle.height *= this->scale;

	}
	RectangleF Camera::calculate_view_rectangle(
		const RectangleF& world_rectangle,
		const Camera& camera)
	{
		return RectangleF((world_rectangle.x - camera.translation.x) * camera.scale,
			(world_rectangle.y - camera.translation.y) * camera.scale,
			world_rectangle.width * camera.scale,
			world_rectangle.height * camera.scale);
	}
	void Camera::calculate_view_rectangle(
		const RectangleF& world_rectangle,
		const Camera& camera,
		RectangleF& view_rectangle)
	{
		view_rectangle.x = (world_rectangle.x - camera.translation.x) * camera.scale;
		view_rectangle.y = (world_rectangle.y - camera.translation.y) * camera.scale;
		view_rectangle.width = world_rectangle.width * camera.scale;
		view_rectangle.height = world_rectangle.height * camera.scale;
	}
	Camera Camera::calculate_intermediate_camera(const Camera& first, const Camera& last, float amount)
	{
		return Camera(Vector2F::lerp(first.translation, last.translation, amount),
			lerp(first.scale, last.scale, amount));
	}
	Camera Camera::calculate_camera_from_view_rectangle(
		const RectangleF& view_rectangle,
		const RectangleF& world_rectangle)
	{
		//return Camera(world_rectangle.x - view_rectangle.x,
		//	world_rectangle.y - view_rectangle.y,
		//	world_rectangle.width / view_rectangle.width);


		return Camera(view_rectangle.x - world_rectangle.x,
			view_rectangle.y - world_rectangle.y,
			world_rectangle.width / view_rectangle.width);

	}
	Vector2F Camera::calculate_view_position(
		const Vector2F& world_position) const
	{
		return Vector2F((world_position.x - this->translation.x) * this->scale,
			(world_position.y - this->translation.y) * this->scale);
	}
	float Camera::calculate_view_scale(float world_scale) const
	{
		return world_scale * this->scale;
	}
	const Camera Camera::DEFAULT_CAMERA = { Vector2F::ZERO, 1.0f };

#pragma endregion Camera

#pragma region RectangleRotated

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
		if (center_line.get_length() == 0.0f)
		{
			throw std::invalid_argument(
				"RectangleRotated: centre line has zero length, so it has no direction");
		}

		this->center_ = center_line.get_center();
		// Segment::get_direction() returns point_1 - point_0 un-normalised,
		// but axes_valid() requires a unit axis - without this the constructor
		// threw for every segment whose length was not 1.
		this->x_axis_ = center_line.get_direction();
		this->x_axis_.normalize();
		this->y_axis_ = Vector2F::normal(this->x_axis_);
		this->hw_extents_ = Vector2F(center_line.get_length() / 2.0f + thickness, thickness);

		this->points_ = this->calculate_points();

		if (!this->is_valid())
		{
			throw std::invalid_argument("RectangleRotated is not valid");
		}
	}

	RectangleF RectangleRotated::get_bounding_box() const
	{
		float x1 = std::min(std::min(std::min(this->points_[0].x, this->points_[1].x),
			this->points_[2].x), this->points_[3].x);

		float x2 = std::max(std::max(std::max(this->points_[0].x, this->points_[1].x),
			this->points_[2].x), this->points_[3].x);

		float y1 = std::min(std::min(std::min(this->points_[0].y, this->points_[1].y),
			this->points_[2].y), this->points_[3].y);

		float y2 = std::max(std::max(std::max(this->points_[0].y, this->points_[1].y),
			this->points_[2].y), this->points_[3].y);

		return RectangleF(x1, y1, x2 - x1, y2 - y1);
	}
	shape_type RectangleRotated::get_shape_type() const
	{
		return shape_type::RECTANGLE_ROTATED;
	}
	bool RectangleRotated::intersects(const RectangleF& rect) const
	{
		return rectangle_rotated_rectangle_intersect(rect, *this);
	}
	bool RectangleRotated::intersects(const Circle& circle) const
	{
		return circle_rectangle_rotated_intersect(circle, *this);
	}
	bool RectangleRotated::intersects(const Triangle& triangle) const
	{
		return triangle_rectangle_rotated_intersect(triangle, *this);
	}
	bool RectangleRotated::intersects(const Quad& quad) const
	{
		return quad_rectangle_rotated_intersect(quad, *this);
	}
	bool RectangleRotated::intersects(const Segment& segment) const
	{
		return segment_rectangle_rotated_intersect(segment, *this);
	}
	bool RectangleRotated::intersects(const Point2F& point) const
	{
		return point_rectangle_rotated_intersect(point, *this);
	}
	bool RectangleRotated::intersects(const RectangleRotated& rect_rotated) const
	{
		return rectangles_rotated_intersect(*this, rect_rotated);
	}
	bool RectangleRotated::contains(const Point2F& point) const
	{
		return point_rectangle_rotated_intersect(point, *this);
	}
	void RectangleRotated::offset(const Vector2F& amount)
	{
		this->center_ += amount;

		this->points_ = this->calculate_points();

	}
	std::unique_ptr<Shape> RectangleRotated::clone() const
	{
		return std::make_unique<RectangleRotated>(*this);
	}
	Point2F RectangleRotated::get_center() const
	{
		return this->center_;
	}
	std::vector<Segment> RectangleRotated::get_edges() const
	{
		std::vector<Segment> edges =
		{
			this->get_edge_0(),
			this->get_edge_1(),
			this->get_edge_2(),
			this->get_edge_3()
		};
		return edges;
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

	Point2F RectangleRotated::get_x_axis() const
	{
		return this->x_axis_;
	}
	Point2F RectangleRotated::get_y_axis() const
	{
		return this->y_axis_;
	}
	Point2F RectangleRotated::get_axis(int axis) const
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
	Point2F RectangleRotated::get_half_extents() const
	{
		return this->hw_extents_;
	}
	float RectangleRotated::get_half_x_width() const
	{
		return this->hw_extents_.x;
	}
	float RectangleRotated::get_half_y_width() const
	{
		return this->hw_extents_.y;
	}
	float RectangleRotated::get_half_width(int axis) const
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
	Point2F RectangleRotated::get_point_0() const
	{
		return this->points_[0];
	}
	Point2F RectangleRotated::get_point_1() const
	{
		return this->points_[1];
	}
	Point2F RectangleRotated::get_point_2() const
	{
		return this->points_[2];
	}
	Point2F RectangleRotated::get_point_3() const
	{
		return this->points_[3];
	}
	const std::vector<Point2F>& RectangleRotated::get_points() const
	{
		return this->points_;
	}
	Segment RectangleRotated::get_edge_0() const
	{
		return Segment(this->points_[0], this->points_[1]);
	}
	Segment RectangleRotated::get_edge_1() const
	{
		return Segment(this->points_[1], this->points_[2]);
	}
	Segment RectangleRotated::get_edge_2() const
	{
		return Segment(this->points_[2], this->points_[3]);
	}
	Segment RectangleRotated::get_edge_3() const
	{
		return Segment(this->points_[3], this->points_[0]);
	}
	Quad RectangleRotated::get_quad() const
	{
		auto points = this->calculate_points();

		return Quad(points);
	}
	RectangleF RectangleRotated::get_rectangle_rotated_to_axis() const
	{
		return RectangleF(this->center_, this->hw_extents_.x, this->hw_extents_.y);
	}
	float RectangleRotated::get_angle() const
	{
		return Vector2F::angle_between(this->x_axis_, Vector2F::DIRECTION_RIGHT);
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

	std::vector<Point2F> RectangleRotated::calculate_points() const
	{
		return calculate_points(this->center_, this->x_axis_, this->y_axis_, this->hw_extents_);
	}

	std::vector<Point2F> RectangleRotated::calculate_points(const Point2F& center,
		const Vector2F& x_axis, const Vector2F& y_axis,
		const Vector2F& hw_extents) const
	{
		std::vector<Point2F> points(4);

		points[0] = center - x_axis * hw_extents.x - y_axis * hw_extents.y;
		points[1] = center + x_axis * hw_extents.x - y_axis * hw_extents.y;
		points[2] = center + x_axis * hw_extents.x + y_axis * hw_extents.y;
		points[3] = center - x_axis * hw_extents.x + y_axis * hw_extents.y;

		return points;
	}

	std::vector<Point2F> RectangleRotated::calculate_points(const Segment& center_line,
		float thickness) const
	{
		Vector2F center = center_line.get_center();
		Vector2F x_axis = center_line.get_direction();
		x_axis.normalize();
		Vector2F y_axis = Vector2F::normal(x_axis);
		Vector2F hw_extents = Vector2F(center_line.get_length() / 2.0f + thickness, thickness);

		return calculate_points(center, x_axis, y_axis, hw_extents);
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
		std::vector<Segment> edges = this->get_edges();
		if (edges.size() != 4)
		{
			throw std::invalid_argument("RectangleRotated must have 4 edges");
		}

		// Compare unit directions rather than the raw edge vectors. The dot
		// product of two un-normalised edges scales with the square of the
		// rectangle's size, so an absolute epsilon rejected large rectangles
		// that were perfectly square.
		for (size_t i = 0; i < 4; i++)
		{
			const Vector2F direction_a = edges[i].get_direction().normalized();
			const Vector2F direction_b =
				edges[(i + 1) % 4].get_direction().normalized();

			if (!are_equal(Vector2F::dot(direction_a, direction_b), 0.0f, EPSILON))
			{
				return false;
			}
		}

		// Opposite edges must match in length, to a tolerance that scales with
		// the edges themselves for the same reason.
		const float length_0 = edges[0].get_length();
		const float length_1 = edges[1].get_length();

		if (std::fabs(length_0 - edges[2].get_length()) >
				EPSILON * std::max(1.0f, length_0) ||
			std::fabs(length_1 - edges[3].get_length()) >
				EPSILON * std::max(1.0f, length_1))
		{
			return false;
		}

		return true;
	}

#pragma endregion RectangleRotated

} // namespace mattmath