#pragma once

// MattMath depends on nothing (docs/design/ARCHITECTURE.md, the module
// table). It used to carry a conversion to and from the matching DirectXTK
// or D3D11 type on nearly every value here - SimpleMath::Vector2, XMFLOAT2,
// SimpleMath::Rectangle, RECT, SimpleMath::Color, D3D11_VIEWPORT. Counted,
// the whole set had one consumer: engine/render/d3d11/renderer.cpp. So they
// live there now, as free functions at the backend's edge, and the library
// that is supposed to depend on nothing does.

#include "engine/math/shape_type.h"
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace mattmath
{
	enum class Direction
	{
		none,
		up,
		down,
		left,
		right,
		up_left,
		up_right,
		down_left,
		down_right,
	};

	//enum class vector_type
	//{
	//	row,
	//	column,
	//};
	
	struct Vector2I;
	struct Vector2F;
	typedef Vector2F Point2F;
	struct RectangleI;
	struct Circle;
	struct Triangle;
	struct Quad;
	struct Segment;
	struct RectangleF;
	struct RectangleRotated;

	constexpr float PI = 3.14159265358979323846f;
	constexpr float PI_OVER_2 = PI / 2.0f;

	// The general comparison tolerance, and the one number here that carries a
	// warning.
	//
	// EPSILON is ABSOLUTE, so it only means anything over a bounded range of
	// coordinates, and the range it means something over is smaller than the
	// one this game runs at. One float ulp at 5000 is 4.883e-4, so
	// `5000.0f + EPSILON == 5000.0f` exactly: out where the levels are, the
	// engine's general tolerance is a fifth of the smallest representable
	// step, which is to say it is exact equality wearing a costume. The tests
	// mostly run within a few hundred units of the origin, where one ulp is
	// 6.1e-5 and EPSILON is a meaningful ~1.6 of them - so the suite exercises
	// the range where this constant works and the game runs in the range where
	// it does not.
	//
	// That is not currently a bug, and the reason is worth recording so nobody
	// "fixes" it by making the number bigger. Every use of EPSILON in the
	// collision path CLASSIFIES - is this vector degenerate, is this axis
	// usable - and none of them MEASURE. The one place a tolerance would have
	// moved geometry was resolve.cpp's guard, and that now refuses rather than
	// returns a number (see MIN_AXIS_ALIGNMENT). The separation sweep was
	// re-run translated out to 600,000 units and holds exactly, so the
	// analytic path needs no tolerance at all at world scale.
	//
	// The ordering, which is what Ericson insists a set of tolerances has
	// (8.4.3, p.377 - a query tolerance must exceed the tolerance geometry was
	// built with, or a primitive placed on one side is missed by a strict
	// query). Smallest first:
	//
	//   SEGMENT_PARALLEL_EPSILON  1e-6  ericson_math.cpp, anonymous namespace.
	//                                   A slab-test fudge for a segment nearly
	//                                   parallel to an axis. Two orders tighter
	//                                   than EPSILON on purpose; they were
	//                                   never the same quantity.
	//   EPSILON                   1e-4  this constant. Classification only.
	//   MIN_AXIS_ALIGNMENT        0.1   resolve.h. Not a rounding tolerance at
	//                                   all - a bound on how oblique an axis
	//                                   may be before separating along it is a
	//                                   category error. Named here because it
	//                                   is the one that decides whether a
	//                                   caller gets an answer.
	//
	// Anything added to this set states which of those three jobs it does, and
	// where it sits in the order. A tolerance that may move geometry has to be
	// strictly larger than one that may only classify it.
	constexpr float EPSILON = 0.0001f;

	//float min_value(float a, float b);
	//float max_value(float a, float b);

	float clamp(float value, float min, float max);
	void clamp_ref(float& value, float min, float max);
	int clamp(int value, int min, int max);
	void clamp_ref(int& value, int min, int max);

	bool are_equal(float a, float b, float epsilon = EPSILON);
	bool are_equal(const mattmath::Vector2F& a, const mattmath::Vector2F& b,
		float epsilon = 0.0001f);

	float to_radians(float degrees);
	float to_degrees(float radians);

	float lerp(float a, float b, float t);
	
	struct Shape
	{
		virtual ~Shape() = default;
		virtual RectangleF bounding_box() const = 0;
		virtual ShapeType shape_type() const = 0;
		virtual bool intersects(const RectangleF& rect) const = 0;
		virtual bool intersects(const Circle& circle) const = 0;
		virtual bool intersects(const Triangle& triangle) const = 0;
		virtual bool intersects(const Quad& quad) const = 0;
		virtual bool intersects(const Segment& segment) const = 0;
		virtual bool intersects(const Point2F& point) const = 0;
		virtual bool intersects(const RectangleRotated& rect_rotated) const = 0;
		bool intersects(const Shape* other) const;
		bool intersects(const Shape& other) const;
		bool AABB_intersects(const Shape* other) const;
		bool AABB_intersects(const Shape& other) const;
		virtual void offset(const Vector2F& amount) = 0;
		virtual Point2F center() const = 0;
		// Grows the shape by moving every part of its boundary `amount`
		// outward, along that part's own normal.
		//
		// One contract for all four implementations, because a virtual with
		// three different meanings is worse than three differently named
		// functions. A box's faces each move out by `amount`; a circle's
		// radius grows by `amount`; a polygon's edges each move out by
		// `amount` and its corners are extended to meet (a mitre).
		//
		// The result always CONTAINS the original. That direction is the
		// contract, not an accident of the arithmetic - a collider that grows
		// by less than it was asked to lets objects visibly interpenetrate
		// while the collision system correctly reports no touch, which is the
		// one failure a geometry simplifier must never have. Polygons used to
		// have exactly that bug: they displaced each vertex away from the
		// centroid by `amount`, which moves the adjacent edges out by only
		// amount * cos(angle between the vertex ray and the edge normal) -
		// short of the request, by a different factor at every corner.
		//
		// The true offset of a polygon has arcs where the corners were; the
		// mitre keeps the result a polygon and errs outward, which is the safe
		// side (T3 - nobody will see the corner).
		//
		// A negative `amount` is not supported: shrinking can invert a small
		// polygon through itself, and no caller wants it.
		virtual void inflate(float amount) = 0;

		// NOT HERE: clone(), and edges().
		//
		// A polymorphic clone() on the engine's most-copied value was filed
		// `high` twice, and counting said it had one caller in the whole
		// repository: a Structure constructor that took a borrowed
		// `const Shape*` and secretly copied it. That constructor takes a
		// unique_ptr now, so the copy is at the call site where it is visible
		// and the virtual has nothing left to serve. Every other shape here is
		// copied by its own copy constructor, which is what a value does.
		//
		// edges() was a pure virtual returning std::vector<Segment> - a heap
		// allocation per call for three or four segments known at compile
		// time - and it was called only through concrete types, never through
		// a Shape&. So the polymorphism paid for nothing and the allocation
		// paid for less: the callers are the intersection routines below, on
		// the narrow phase's own path. Each shape declares its own edges()
		// returning std::array of the right length, and Circle - which
		// answered the pure virtual with an empty vector because a circle has
		// no edges - declares none at all.
	};

	bool shapes_intersect(const Shape* a, const Shape* b);
	bool shapes_intersect(const Shape& a, const Shape& b);
	bool shapes_AABB_intersect(const Shape* a, const Shape* b);
	bool shapes_AABB_intersect(const Shape& a, const Shape& b);

	bool rectangles_intersect(const mattmath::RectangleF& a,
		const mattmath::RectangleF& b);

	bool rectangle_circle_intersect(const mattmath::RectangleF& rectangle,
		const mattmath::Circle& circle, mattmath::Point2F& point);

	bool rectangle_circle_intersect(const mattmath::RectangleF& rectangle,
		const mattmath::Circle& circle);

	bool rectangle_triangle_intersect(const mattmath::RectangleF& rectangle, 
		const mattmath::Triangle& triangle);

	bool rectangle_quad_intersect(const mattmath::RectangleF& rectangle,
		const mattmath::Quad& quad);

	bool rectangle_segment_intersect(const mattmath::RectangleF& rectangle, 
		const mattmath::Segment& segment);

	bool rectangle_point_intersect(const mattmath::RectangleF& rectangle, 
		const mattmath::Point2F& point);

	bool rectangle_rotated_rectangle_intersect(const mattmath::RectangleF& rect,
		const mattmath::RectangleRotated& rotated_rect);

	bool circles_intersect(const mattmath::Circle& a,
		const mattmath::Circle& b);

	bool circle_triangle_intersect(const mattmath::Circle& circle,
		const mattmath::Triangle& triangle, mattmath::Point2F& point);

	bool circle_triangle_intersect(const mattmath::Circle& circle,
		const mattmath::Triangle& triangle);

	bool circle_quad_intersect(const mattmath::Circle& circle,
		const mattmath::Quad& quad);

	bool circle_segment_intersect(const mattmath::Circle& circle,
		const mattmath::Segment& segment, mattmath::Point2F& point);

	bool circle_segment_intersect(const mattmath::Circle& circle,
		const mattmath::Segment& segment);

	bool circle_point_intersect(const mattmath::Circle& circle, 
		const mattmath::Point2F& point);

	bool circle_rectangle_rotated_intersect(const mattmath::Circle& circle,
		const mattmath::RectangleRotated& rect_rotated);

	bool triangles_intersect(const mattmath::Triangle& a, 
		const mattmath::Triangle& b);

	bool triangle_quad_intersect(const mattmath::Triangle& triangle, 
		const mattmath::Quad& quad);

	bool triangle_segment_intersect(const mattmath::Triangle& triangle, 
		const mattmath::Segment& segment);

	bool triangle_point_intersect(const mattmath::Triangle& triangle,
		const mattmath::Point2F& point);

	bool triangle_rectangle_rotated_intersect(const mattmath::Triangle& triangle,
		const mattmath::RectangleRotated& rect_rotated);

	bool quads_intersect(const mattmath::Quad& a, const mattmath::Quad& b);

	bool quad_segment_intersect(const mattmath::Quad& quad,
		const mattmath::Segment& segment);

	bool quad_point_intersect(const mattmath::Quad& quad, 
		const mattmath::Point2F& point);

	bool quad_rectangle_rotated_intersect(const mattmath::Quad& quad, 
		const mattmath::RectangleRotated& rect_rotated);

	bool segments_intersect(const mattmath::Segment& a, const mattmath::Segment& b,
		float& t, mattmath::Point2F& p);

	bool segments_intersect(const mattmath::Segment& a, const mattmath::Segment& b);

	bool segment_rectangle_rotated_intersect(const mattmath::Segment& segment,
		const mattmath::RectangleRotated& rect_rotated);

	bool point_rectangle_rotated_intersect(const mattmath::Point2F& point,
		const mattmath::RectangleRotated& rect_rotated);

	bool rectangles_rotated_intersect(const mattmath::RectangleRotated& a,
		const mattmath::RectangleRotated& b);

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
		//RectangleF(const mattmath::Segment& center_line, float thickness);

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
		float area() const;
		mattmath::RectangleI rectangle_i() const;

		bool operator==(const RectangleF& other) const;
		bool operator!=(const RectangleF& other) const;

		//bool contains(const mattmath::Vector2F& point) const;
		bool contains(const RectangleF& other) const;

		bool intersects(const RectangleF& other) const override;
		bool intersects(const mattmath::Circle& other) const override;
		bool intersects(const mattmath::Triangle& other) const override;
		bool intersects(const mattmath::Quad& quad) const override;
		bool intersects(const mattmath::Segment& other) const override;
		bool intersects(const mattmath::Point2F& point) const override;
		bool intersects(const RectangleRotated& rect_rotated) const override;
		bool contains(const mattmath::Point2F& point) const;
		RectangleF intersection(const RectangleF& other) const;
		
		//void set_left(float left);
		//void set_top(float top);
		//void set_right(float right);
		//void set_bottom(float bottom);

		void inflate(float horizontal_amount, float vertical_amount);
		void inflate(const mattmath::Vector2F& amount);
		void inflate(float amount) override;
		void inflate_to_size(float width, float height);
		void inflate_to_size(const mattmath::Vector2F& size);
		void scale_at_center(float scale);
		void scale_at_center(float horizontal_scale, float vertical_scale);
		void scale_at_center(const mattmath::Vector2F& scale);
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

		static RectangleF intersection(const RectangleF& a, const RectangleF& b);
		static RectangleF union_of(const RectangleF& a, const RectangleF& b);

		static RectangleF from_top_left_bottom_right(const mattmath::Vector2F& top_left,
			const mattmath::Vector2F& bottom_right);
		static RectangleF from_top_left_bottom_right(float top, float left,
			float bottom, float right);

		static const RectangleF ZERO;
	};

	typedef mattmath::RectangleF AABB;
	
	/*template<typename T>
	struct Matrix
	{
		Matrix() = default;
		Matrix(const Matrix&) = default;
		Matrix(int rows, int columns);
		Matrix(int rows, int columns, const std::vector<T>& elements);
		Matrix(int size, vector_type type);
		Matrix(int size, vector_type type, const std::vector<T>& elements);

		T element(int row, int column) const;
		void set_element(int row, int column, T value);

		int rows() const;
		int columns() const;

		bool is_square() const;
		bool equal_size(const Matrix<T>& other) const;

		Matrix<T> rotate_pi_radians() const;

		T& operator()(int row, int column);
		const T& operator()(int row, int column) const;

	private:
		int rows_ = 0;
		int columns_ = 0;
		std::vector<T> elements_;
		T** matrix_ = nullptr;
		bool row_valid(int row) const;
		bool column_valid(int column) const;
		int calculate_index(int row, int column) const;
		T& element_ref(int row, int column);
	};

	template<typename T>
	struct Vector : Matrix<T>
	{
		Vector() = default;
		Vector(const Vector&) = default;
		Vector(int size);
		Vector(int size, vector_type type);
		Vector(int size, vector_type type, const std::vector<T>& elements);

		int size() const;
		vector_type vector_type() const;

		T& operator[](int index);
		const T& operator[](int index) const;
	};*/

	struct Vector2I
	{
		int x = 0;
		int y = 0;

		Vector2I() = default;
		Vector2I(const Vector2I&) = default;
		Vector2I(int x, int y);
		Vector2I(const mattmath::Vector2F& vector);

		bool operator==(const Vector2I& other) const;
		bool operator!=(const Vector2I& other) const;

		Vector2I& operator+=(const Vector2I& other);
		Vector2I& operator-=(const Vector2I& other);
		Vector2I& operator*=(const Vector2I& other);
		Vector2I& operator/=(const Vector2I& other);
		Vector2I& operator*=(int other);
		Vector2I& operator/=(int other);

		//Vector2I operator+(const Vector2I& other) const;
		//Vector2I operator-(const Vector2I& other) const;

		void offset(int horizontal_amount, int vertical_amount);
		void scale(int horizontal_amount, int vertical_amount);
		void set(int x, int y);

		static const Vector2I ZERO;
	};

	Vector2I operator+ (const Vector2I& V1, const Vector2I& V2);
	Vector2I operator- (const Vector2I& V1, const Vector2I& V2);
	Vector2I operator* (const Vector2I& V1, const Vector2I& V2);	
	Vector2I operator* (const Vector2I& V, int S);
	Vector2I operator/ (const Vector2I& V1, const Vector2I& V2);
	Vector2I operator/ (const Vector2I& V, int S);
	Vector2I operator* (int S, const Vector2I& V);

	struct Vector2F
	{
		float x = 0.0f;
		float y = 0.0f;

		Vector2F() = default;
		Vector2F(const Vector2F&) = default;
		Vector2F(float f);
		Vector2F(float x, float y);
		Vector2F(const mattmath::Vector2I& vector);

		bool operator==(const Vector2F& other) const;
		bool operator!=(const Vector2F& other) const;

		Vector2F& operator+=(const Vector2F& other);
		Vector2F& operator-=(const Vector2F& other);
		Vector2F& operator*=(const Vector2F& other);
		Vector2F& operator/=(const Vector2F& other);
		Vector2F& operator*=(float other);
		Vector2F& operator/=(float other);

		//Vector2F& operator/(float other);
		//Vector2F& operator*(float other);

		//Vector2F& operator+(const Vector2F& other);
		//Vector2F& operator-(const Vector2F& other);
		//Vector2F& operator*(const Vector2F& other);
		//Vector2F& operator/(const Vector2F& other);

		float length() const;
		float length_squared() const;
		mattmath::Direction direction() const;

		float dot(const Vector2F& other) const;

		// Zero-length in, zero-length out. A zero vector has no direction, and
		// returning zero lets the caller detect that; dividing by the length
		// produced NaN, which then propagated silently through velocities and
		// shape validation.
		//
		// Note this is deliberately NOT the same contract as to_unit_vector()
		// and unit_vector(), which substitute (1, 0) for a zero vector. Use
		// those only where an arbitrary direction is acceptable.
		Vector2F normalized() const;
		void normalize();

		bool is_contained_within(const mattmath::RectangleF& other) const;

		void clamp(const Vector2F& min, const Vector2F& max);
		Vector2F clamped(const Vector2F& min, const Vector2F& max) const;

		float angle() const;
		static float angle(const Vector2F& vec);

		void rotate(float angle);
		void normal();

		// Substitutes (1, 0) for a zero-length vector - i.e. it invents a
		// direction rather than reporting that there is none. See normalize().
		void to_unit_vector();

		bool abs_x_greater_than_y() const;

		static Vector2F rotate_vector(const Vector2F& vec, float angle);

		static float angle_between(const Vector2F& a, const Vector2F& b);

		static Vector2F lerp(const Vector2F& a, const Vector2F& b, float t);
		static Vector2F lerp(const Vector2F& a, const Vector2F& b, const Vector2F& t);

		static float distance(const Vector2F& a, const Vector2F& b);
		static float distance_squared(const Vector2F& a, const Vector2F& b);

		static float dot(const Vector2F& a, const Vector2F& b);

		// The 2D cross product: a scalar, not a vector. In three dimensions
		// the cross product of two vectors is the third axis; in two there is
		// no third axis, and what survives is its signed length,
		// a.x * b.y - a.y * b.x.
		//
		// It is the signed area of the parallelogram a and b span, so its
		// magnitude measures how far from parallel they are - the exact
		// counterpart of dot() measuring how far from perpendicular. Its sign
		// is the orientation test: positive when b lies counter-clockwise of
		// a, negative when clockwise, and zero when the two are parallel,
		// which is the only reliable way to ask that question.
		//
		// Comparing three points is this on their differences:
		// cross(b - a, c - a) is twice the signed area of triangle abc, and
		// is positive exactly when abc winds counter-clockwise. That
		// composite is Ericson's ORIENT2D; ericson_math.h spells it
		// signed_2D_tri_area.
		//
		// Beware the y-down screen convention: "counter-clockwise" above is
		// stated in maths axes. On screen, where y grows downward, a positive
		// result reads as clockwise. The sign is consistent either way - it
		// is the word that flips.
		static float cross(const Vector2F& a, const Vector2F& b);

		static Vector2F min_vec(const Vector2F& a, const Vector2F& b);
		static Vector2F max_vec(const Vector2F& a, const Vector2F& b);

		static Vector2F vec_from_angle_magnitude(float angle, float magnitude);
		static Vector2F unit_vec_from_angle(float angle);

		// Returns (1, 0) for a zero-length vector. See to_unit_vector().
		static Vector2F unit_vector(const Vector2F& vec);
		
		static Vector2F normal(const Vector2F& vec);

		static Vector2F direction_to_8_cardinal_direction(const Vector2F& direction);

		static const Vector2F ZERO;
		static const Vector2F ONE;
		static const Vector2F DIRECTION_RIGHT;
		static const Vector2F DIRECTION_DOWN;
		static const Vector2F DIRECTION_LEFT;
		static const Vector2F DIRECTION_UP;
		static const Vector2F DIRECTION_UP_RIGHT;
		static const Vector2F DIRECTION_DOWN_RIGHT;
		static const Vector2F DIRECTION_DOWN_LEFT;
		static const Vector2F DIRECTION_UP_LEFT;
	};

	Vector2F operator+ (const Vector2F& V1, const Vector2F& V2);
	Vector2F operator- (const Vector2F& V1, const Vector2F& V2);
	Vector2F operator* (const Vector2F& V1, const Vector2F& V2);
	Vector2F operator* (const Vector2F& V, float S);
	Vector2F operator/ (const Vector2F& V1, const Vector2F& V2);
	Vector2F operator/ (const Vector2F& V, float S);
	Vector2F operator* (float S, const Vector2F& V);

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
		RectangleI(mattmath::RectangleF rectangle);

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

	struct Vector4F
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float w = 0.0f;

		Vector4F() = default;
		Vector4F(const Vector4F&) = default;
		Vector4F(float x, float y, float z, float w);

		bool operator==(const Vector4F& other) const;
		bool operator!=(const Vector4F& other) const;
	};

	struct Vector3F
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;

		Vector3F() = default;
		Vector3F(const Vector3F&) = default;
		Vector3F(float x, float y, float z);

		bool operator==(const Vector3F& other) const;
		bool operator!=(const Vector3F& other) const;

		Vector3F& operator+=(const Vector3F& other);
		Vector3F& operator-=(const Vector3F& other);
		Vector3F& operator*=(const Vector3F& other);
		Vector3F& operator/=(const Vector3F& other);
		Vector3F& operator*=(float other);
		Vector3F& operator/=(float other);
	};

	Vector3F operator+ (const Vector3F& V1, const Vector3F& V2);
	Vector3F operator- (const Vector3F& V1, const Vector3F& V2);
	Vector3F operator* (const Vector3F& V1, const Vector3F& V2);
	Vector3F operator* (const Vector3F& V, float S);
	Vector3F operator/ (const Vector3F& V1, const Vector3F& V2);
	Vector3F operator/ (const Vector3F& V, float S);
	Vector3F operator* (float S, const Vector3F& V);

	struct Circle : public Shape
	{
		// Private, unlike the other shapes' data, because Shape's polymorphic
		// accessor is center() and a field of that name cannot coexist with
		// it. radius() is here to keep the pair symmetrical.

		Circle() = default;
		Circle(const Circle&) = default;
		Circle(const mattmath::Vector2F& center, float radius);
		Circle(float x, float y, float radius);

		mattmath::RectangleF bounding_box() const override;
		ShapeType shape_type() const override;
		void offset(const mattmath::Vector2F& amount) override;
		void inflate(float amount) override;

		bool operator==(const Circle& other) const;
		bool operator!=(const Circle& other) const;

		//bool contains(const mattmath::Vector2F& point) const;

		bool intersects(const mattmath::RectangleF& other) const override;
		bool intersects(const Circle& other) const override;
		bool intersects(const mattmath::Triangle& other) const override;
		bool intersects(const mattmath::Quad& other) const override;
		bool intersects(const mattmath::Segment& other) const override;
		bool intersects(const mattmath::Point2F& point) const override;
		bool intersects(const RectangleRotated& rect_rotated) const override;
		bool contains(const mattmath::Point2F& point) const;

		mattmath::Vector2F center() const override;
		float radius() const;

	private:
		mattmath::Vector2F center_ = mattmath::Vector2F::ZERO;
		float radius_ = 0.0f;
	};
	struct Triangle : public Shape
	{
		Vector2F points[3] = { Vector2F::ZERO, Vector2F::ZERO, Vector2F::ZERO };

		Triangle() = default;
		Triangle(const Triangle&) = default;
		Triangle(const mattmath::Vector2F& point0,
			const mattmath::Vector2F& point1,
			const mattmath::Vector2F& point2);
		Triangle(float x0, float y0, float x1, float y1, float x2, float y2);

		mattmath::RectangleF bounding_box() const override;
		ShapeType shape_type() const override;
		void offset(const mattmath::Vector2F& amount) override;
		void inflate(float amount) override;

		const Vector2F& point_0() const;
		const Vector2F& point_1() const;
		const Vector2F& point_2() const;

		Segment edge_0() const;
		Segment edge_1() const;
		Segment edge_2() const;
		std::array<Segment, 3> edges() const;

		float angle_0() const;
		float angle_1() const;
		float angle_2() const;
		std::vector<float> angles() const;

		bool operator==(const Triangle& other) const;
		bool operator!=(const Triangle& other) const;

		//bool contains(const mattmath::Vector2F& point) const;

		bool intersects(const mattmath::RectangleF& other) const override;
		bool intersects(const mattmath::Circle& other) const override;
		bool intersects(const Triangle& other) const override;
		bool intersects(const mattmath::Quad& other) const override;
		bool intersects(const mattmath::Segment& other) const override;
		bool intersects(const mattmath::Point2F& point) const override;
		bool intersects(const RectangleRotated& rect_rotated) const override;
		bool contains(const mattmath::Point2F& point) const;

		mattmath::Vector2F center() const override;

		float calculate_gradient(int edge) const;
	};
	struct TriangleRightAxisAligned : public Triangle
	{
		TriangleRightAxisAligned() = default;
		TriangleRightAxisAligned(const TriangleRightAxisAligned&) = default;
		TriangleRightAxisAligned(const mattmath::Vector2F& top,
			const mattmath::Vector2F& left, const mattmath::Vector2F& right);
		TriangleRightAxisAligned(float x0, float y0, float x1, float y1,
			float x2, float y2);

		Segment hypotenuse() const;
		float hypotenuse_gradient() const;

	private:
		//bool is_right_triangle(const Triangle& tri) const;
		int find_hypotenuse(const Triangle& tri) const;
		float calculate_gradient(int edge) const;
	};

	/*
	* A quadrilateral with four points.
	* The points are ordered in a clockwise direction, starting from the top left.
	*/
	struct Quad : public Shape
	{
		Quad() = default;
		Quad(const Quad&) = default;
		Quad(const Vector2F& point1, const Vector2F& point2,
			const Vector2F& point3, const Vector2F& point4);
		Quad(const std::vector<Point2F>& points);
		Quad(const RectangleF& rectangle);
		Quad(const RectangleRotated& rectangle);

		mattmath::RectangleF bounding_box() const override;
		ShapeType shape_type() const override;
		void offset(const mattmath::Vector2F& amount) override;
		void inflate(float amount) override;

		bool is_valid() const;

		const Point2F& point_0() const;
		const Point2F& point_1() const;
		const Point2F& point_2() const;
		const Point2F& point_3() const;
		std::vector<Point2F> points() const;

		void set_point_0(const Point2F& point);
		void set_point_1(const Point2F& point);
		void set_point_2(const Point2F& point);
		void set_point_3(const Point2F& point);

		Segment edge_0() const;
		Segment edge_1() const;
		Segment edge_2() const;
		Segment edge_3() const;
		std::array<Segment, 4> edges() const;

		Triangle triangle_0() const;
		Triangle triangle_1() const;
		std::vector<Triangle> triangles() const;

		bool operator==(const Quad& other) const;
		bool operator!=(const Quad& other) const;

		bool intersects(const RectangleF& other) const override;
		bool intersects(const Circle& other) const override;
		bool intersects(const Triangle& other) const override;
		bool intersects(const Quad& other) const override;
		bool intersects(const Segment& other) const override;
		bool intersects(const Point2F& point) const override;
		bool intersects(const RectangleRotated& rect_rotated) const override;
		bool contains(const Point2F& point) const;

		mattmath::Vector2F center() const override;

	private:
		Point2F points_[4] = { Vector2F::ZERO, Vector2F::ZERO,
					Vector2F::ZERO, Vector2F::ZERO };
	};

	struct Segment
	{
		mattmath::Point2F point_0 = mattmath::Point2F::ZERO;
		mattmath::Point2F point_1 = mattmath::Point2F::ZERO;

		Segment() = default;
		Segment(const Segment&) = default;
		Segment(const mattmath::Point2F& point_0,
			const mattmath::Point2F& point_1);
		Segment(float x0, float y0, float x1, float y1);

		bool operator==(const Segment& other) const;
		bool operator!=(const Segment& other) const;

		bool intersects(const Segment& other) const;
		bool intersects(const mattmath::RectangleF& other) const;

		mattmath::Vector2F direction() const;

		float length() const;

		mattmath::Point2F center() const;
		//bool intersects(const mattmath::Circle& other) const;
		//bool intersects(const mattmath::Triangle& other) const;
	};
	struct RectangleRotated : public Shape
	{
		RectangleRotated() = default;
		RectangleRotated(const RectangleRotated&) = default;
		RectangleRotated(const mattmath::Point2F& center,
			const mattmath::Vector2F& x_axis, const mattmath::Vector2F& y_axis,
			const mattmath::Vector2F& hw_extents);
		RectangleRotated(const mattmath::Segment& center_line, float thickness);

		RectangleF bounding_box() const override;
		ShapeType shape_type() const override;
		bool intersects(const RectangleF& rect) const override;
		bool intersects(const Circle& circle) const override;;
		bool intersects(const Triangle& triangle) const override;
		bool intersects(const Quad& quad) const override;
		bool intersects(const Segment& segment) const override;
		bool intersects(const Point2F& point) const override;
		bool intersects(const RectangleRotated& rect_rotated) const override;
		bool contains(const Point2F& point) const;
		void offset(const Vector2F& amount) override;
		Point2F center() const override;
		std::array<Segment, 4> edges() const;
		void inflate(float amount) override;

		Point2F x_axis() const;
		Point2F y_axis() const;
		Point2F axis(int axis) const;
		Point2F half_extents() const;
		float half_x_width() const;
		float half_y_width() const;
		float half_width(int axis) const;

		void set_center(const Point2F& center);
		void set_x_axis(const Point2F& x_axis);
		void set_y_axis(const Point2F& y_axis);
		void set_half_extents(const Point2F& hw_extents);
		void set_half_x_width(float half_x_width);
		void set_half_y_width(float half_y_width);

		Point2F point_0() const;
		Point2F point_1() const;
		Point2F point_2() const;
		Point2F point_3() const;
		const std::vector<Point2F>& points() const;

		Segment edge_0() const;
		Segment edge_1() const;
		Segment edge_2() const;
		Segment edge_3() const;

		Quad quad() const;

		RectangleF rectangle_rotated_to_axis() const;

		float angle() const;
		//RectangleRotated(const mattmath::Point2F& center,
		//	float angle, const mattmath::Vector2F& hw_extents);

		//std::vector<Point2F> points() const;

		bool is_valid() const;

		bool operator==(const RectangleRotated& other) const;
		bool operator!=(const RectangleRotated& other) const;

	private:
		Point2F center_ = Point2F::ZERO;
		Vector2F x_axis_ = Vector2F::DIRECTION_RIGHT;
		Vector2F y_axis_ = Vector2F::DIRECTION_UP;
		Vector2F hw_extents_ = Vector2F::ZERO;

		std::vector<Point2F> points_ = { Point2F::ZERO, Point2F::ZERO,
					Point2F::ZERO, Point2F::ZERO };

		std::vector<Point2F> calculate_points() const;

		std::vector<mattmath::Point2F> calculate_points(const mattmath::Point2F& center,
			const mattmath::Vector2F& x_axis, const mattmath::Vector2F& y_axis,
			const mattmath::Vector2F& hw_extents) const;

		std::vector<mattmath::Point2F> calculate_points(const mattmath::Segment& center_line,
			float thickness) const;

		bool half_widths_valid() const;
		bool axes_valid() const;
		bool edges_valid() const;
	};

	typedef RectangleRotated OBB;
}
