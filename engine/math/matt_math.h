#pragma once

#include "SimpleMath.h"
#include "engine/math/shape_type.h"
#include <d3d11.h>
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
	struct Camera;
	struct Colour;
	struct RectangleRotated;

	constexpr float PI = 3.14159265358979323846f;
	constexpr float PI_OVER_2 = PI / 2.0f;
	constexpr float EPSILON = 0.0001f;

	//float min_value(float a, float b);
	//float max_value(float a, float b);

	float clamp(float value, float min, float max);
	void clamp_ref(float& value, float min, float max);
	int clamp(int value, int min, int max);
	void clamp_ref(int& value, int min, int max);

	int sign(const mattmath::Vector2F& p1,
		const mattmath::Vector2F& p2, const mattmath::Vector2F& p3);

	bool are_equal(float a, float b, float epsilon = EPSILON);
	bool are_equal(const mattmath::Vector2F& a, const mattmath::Vector2F& b,
		float epsilon = 0.0001f);

	float to_radians(float degrees);
	float to_degrees(float radians);

	float lerp(float a, float b, float t);
	
	struct Shape
	{
		virtual ~Shape() = default;
		virtual RectangleF get_bounding_box() const = 0;
		virtual ShapeType get_shape_type() const = 0;
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
		virtual std::unique_ptr<Shape> clone() const = 0;
		virtual Point2F get_center() const = 0;
		virtual std::vector<Segment> get_edges() const = 0;
		virtual void inflate(float amount) = 0;
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
		RectangleF(const DirectX::SimpleMath::Vector2& position,
						const DirectX::SimpleMath::Vector2& size);
		RectangleF(const DirectX::SimpleMath::Rectangle& rectangle);
		RectangleF(const RECT& rectangle);
		RectangleF(const mattmath::Vector2F& center, float horiz_half_width,
			float vert_half_height);
		//RectangleF(const mattmath::Segment& center_line, float thickness);

		RectangleF get_bounding_box() const override;
		ShapeType get_shape_type() const override;
		std::unique_ptr<Shape> clone() const override;

		float get_x() const;
		float get_y() const;
		float get_width() const;
		float get_height() const;
		mattmath::Vector2F get_center() const override;
		mattmath::Vector2F get_position() const;
		mattmath::Vector2F get_size() const;
		float get_left() const;
		float get_right() const;
		float get_top() const;
		float get_bottom() const;
		mattmath::Vector2F get_top_left() const;
		mattmath::Vector2F get_bottom_right() const;
		mattmath::Vector2F get_top_right() const;
		mattmath::Vector2F get_bottom_left() const;
		mattmath::Segment get_top_edge() const;
		mattmath::Segment get_bottom_edge() const;
		mattmath::Segment get_left_edge() const;
		mattmath::Segment get_right_edge() const;
		std::vector<mattmath::Segment> get_edges() const override;
		float get_area() const;
		mattmath::RectangleI get_rectangle_i() const;
		DirectX::SimpleMath::Rectangle get_sm_rectangle() const;
		RECT get_win_rect() const;

		//RectangleF& operator=(const DirectX::SimpleMath::Rectangle& rectangle);
		//RectangleF& operator=(const RECT& rectangle);
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
		void scale_size_and_position(float horizontal_amount,
			float vertical_amount);
		void scale_size_and_position(const mattmath::Vector2F& amount);
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

		T get_element(int row, int column) const;
		void set_element(int row, int column, T value);

		int get_rows() const;
		int get_columns() const;

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
		T& get_element_ref(int row, int column);
	};

	template<typename T>
	struct Vector : Matrix<T>
	{
		Vector() = default;
		Vector(const Vector&) = default;
		Vector(int size);
		Vector(int size, vector_type type);
		Vector(int size, vector_type type, const std::vector<T>& elements);

		int get_size() const;
		vector_type get_vector_type() const;

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
		Vector2I(const DirectX::SimpleMath::Vector2& vector);
		Vector2I(const DirectX::XMINT2& vector);

		DirectX::SimpleMath::Vector2 get_sm_vector() const;
		DirectX::XMINT2 get_xm_vector() const;

		Vector2I& operator=(const DirectX::SimpleMath::Vector2& vector);
		Vector2I& operator=(const DirectX::XMINT2& vector);
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
		void scale(const DirectX::SimpleMath::Vector2& amount);
		void set(int x, int y);
		void set(const DirectX::SimpleMath::Vector2& vector);
		void set(const DirectX::XMINT2& vector);

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
		Vector2F(const DirectX::SimpleMath::Vector2& vector);
		Vector2F(const DirectX::XMFLOAT2& vector);
		Vector2F(const mattmath::Vector2I& vector);

		DirectX::SimpleMath::Vector2 get_sm_vector() const;
		DirectX::XMFLOAT2 get_xm_vector() const;

		Vector2F& operator=(const DirectX::SimpleMath::Vector2& vector);
		Vector2F& operator=(const DirectX::XMFLOAT2& vector);
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
		mattmath::Direction get_direction() const;

		float dot(const Vector2F& other) const;
		Vector2F cross(const Vector2F& other) const;
		
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
		static void rotate_vector_by_ref(Vector2F& vec, float angle);

		static float angle_between(const Vector2F& a, const Vector2F& b);

		static Vector2F lerp(const Vector2F& a, const Vector2F& b, float t);
		static Vector2F lerp(const Vector2F& a, const Vector2F& b, const Vector2F& t);

		static float distance(const Vector2F& a, const Vector2F& b);
		static float distance_squared(const Vector2F& a, const Vector2F& b);

		static float dot(const Vector2F& a, const Vector2F& b);

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
		RectangleI(const DirectX::SimpleMath::Vector2& position,
			const DirectX::SimpleMath::Vector2& size);
		RectangleI(mattmath::RectangleF rectangle);
		RectangleI(const DirectX::SimpleMath::Rectangle& rectangle);
		RectangleI(const RECT& rectangle);

		int get_left() const;
		int get_top() const;
		int get_right() const;
		int get_bottom() const;

		mattmath::Vector2I get_position() const;
		mattmath::Vector2I get_size() const;

		mattmath::Vector2I get_top_left() const;
		mattmath::Vector2I get_bottom_right() const;

		DirectX::SimpleMath::Rectangle get_sm_rectangle() const;
		RECT get_win_rect() const;

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
		Vector4F(const DirectX::XMFLOAT4& vector);

		Vector4F& operator=(const DirectX::XMFLOAT4& vector);
		bool operator==(const Vector4F& other) const;
		bool operator!=(const Vector4F& other) const;

		DirectX::XMVECTOR get_xm_vector() const;
	};

	struct Colour
	{
		float r = 0.0f;
		float g = 0.0f;
		float b = 0.0f;
		float a = 0.0f;
		
		Colour() = default;
		Colour(const Colour&) = default;
		Colour(float r, float g, float b);
		Colour(float r, float g, float b, float a);
		Colour(const DirectX::XMFLOAT4& vector);
		Colour(const DirectX::SimpleMath::Color& colour);
		Colour(int r, int g, int b, int a = 255);
		Colour(const std::string& hex);

		Colour& operator=(const DirectX::XMFLOAT4& vector);
		Colour& operator=(const DirectX::SimpleMath::Color& colour);
		Colour& operator=(const mattmath::Vector4F& vector);

		bool operator==(const Colour& other) const;
		bool operator!=(const Colour& other) const;

		Colour& operator+=(const Colour& other);
		Colour& operator-=(const Colour& other);
		Colour& operator*=(const Colour& other);
		Colour& operator*=(float f);
		Colour& operator/=(const Colour& other);
		Colour& operator/=(float f);

		float get_red() const;
		float get_green() const;
		float get_blue() const;
		float get_alpha() const;

		void set_red(float red);
		void set_green(float green);
		void set_blue(float blue);
		void set_alpha(float alpha);

		void set(float red, float green, float blue, float alpha = 1.0f);
		void set_from_int_rgba(int r, int g, int b, int a = 255);
		void set_from_hex(const std::string& hex);

		void saturate(float amount);
		void desaturate(float amount);

		void brighten(float amount);
		void darken(float amount);

		void invert();

		void make_opaque();
		void make_transparent();

		DirectX::SimpleMath::Color get_sm_colour() const;
		DirectX::XMVECTOR get_xm_vector() const;

		void clamp_colours();
	};

	Colour operator+ (const Colour& V1, const Colour& V2);
	Colour operator- (const Colour& V1, const Colour& V2);
	Colour operator* (const Colour& V1, const Colour& V2);
	Colour operator* (const Colour& V, float S);
	Colour operator/ (const Colour& V1, const Colour& V2);
	Colour operator/ (const Colour& V, float S);
	Colour operator* (float S, const Colour& V);

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

	struct MatrixF
	{
		MatrixF() = default;
		MatrixF(const MatrixF&) = default;
		MatrixF(int rows, int columns);
		MatrixF(int rows, int columns, const std::vector<float>& elements);
		
		float get_element(int row, int column) const;
		void set_element(int row, int column, float value);

		int get_rows() const;
		int get_columns() const;

		// x = columns, y = rows
		Vector2I get_dimensions() const;

		bool is_square() const;
		bool is_identity() const;
		bool is_symmetric() const;
		bool is_diagonal() const;
		bool is_upper_triangular() const;
		bool is_lower_triangular() const;
		bool is_invertible() const;
		//bool is_row_echelon_form() const;
		//bool is_reduced_row_echelon_form() const;

		bool operator==(const MatrixF& other) const;
		bool operator!=(const MatrixF& other) const;

		MatrixF& operator+=(const MatrixF& other);
		MatrixF& operator-=(const MatrixF& other);
		MatrixF& operator*=(const MatrixF& other);
		MatrixF& operator/=(const MatrixF& other);
		MatrixF& operator*=(float other);
		MatrixF& operator/=(float other);

	private:
		int rows_ = 0;
		int columns_ = 0;
		std::vector<float> elements_;
		bool row_valid(int row) const;
		bool column_valid(int column) const;
		int calculate_index(int row, int column) const;
	};

	MatrixF operator+ (const MatrixF& a, const MatrixF& b);
	MatrixF operator- (const MatrixF& a, const MatrixF& b);
	MatrixF operator* (const MatrixF& a, const MatrixF& b);
	MatrixF operator/ (const MatrixF& a, const MatrixF& b);

	bool equal_dimensions(const MatrixF& a, const MatrixF& b);
	MatrixF add(const MatrixF& a, const MatrixF& b);
	MatrixF subtract(const MatrixF& a, const MatrixF& b);
	MatrixF multiply(const MatrixF& a, const MatrixF& b);
	MatrixF divide(const MatrixF& a, const MatrixF& b);
	MatrixF multiply(const MatrixF& a, float b);
	MatrixF divide(const MatrixF& a, float b);
	MatrixF gaussian_elimination(const MatrixF& matrix);
	MatrixF transpose(const MatrixF& matrix);
	//static MatrixF inverse(const MatrixF& matrix);
	MatrixF identity(int size);
	MatrixF zero(int rows, int columns);
	float determinant(const MatrixF& matrix);
	//static std::vector<MatrixF> eigenvectors(const MatrixF& matrix);
	//static std::vector<float> eigenvalues(const MatrixF& matrix);

	struct Matrix3x3F : MatrixF
	{
		Matrix3x3F() = default;
		Matrix3x3F(const Matrix3x3F&) = default;
		Matrix3x3F(const std::vector<float>& elements);
		Matrix3x3F(float e11, float e12, float e13,
			float e21, float e22, float e23,
			float e31, float e32, float e33);

		static Matrix3x3F rotation(float angle);
		static Matrix3x3F scale(float x, float y);
		static Matrix3x3F scale(float scale);
		static Matrix3x3F scale(const Vector2F& scale);
		static Matrix3x3F translation(float x, float y);
		static Matrix3x3F translation(const Vector2F& translation);
	};

	struct Viewport
	{
	public:
		float x = 0.0f;
		float y = 0.0f;
		float width = 0.0f;
		float height = 0.0f;
		float minDepth = 0.0f;
		float maxDepth = 1.0f;

		Viewport() {}
		Viewport(const Viewport&) = default;
		Viewport(float x, float y, float width, float height,
			float minDepth = 0.0f, float maxDepth = 1.0f);
		Viewport(const DirectX::SimpleMath::Viewport& viewport);
		Viewport(const mattmath::RectangleF& rectangle,
			float minDepth = 0.0f, float maxDepth = 1.0f);
		Viewport(const mattmath::RectangleI& rectangle,
			float minDepth = 0.0f, float maxDepth = 1.0f);
		Viewport(const D3D11_VIEWPORT& viewport);

		DirectX::SimpleMath::Viewport get_sm_viewport() const;
		D3D11_VIEWPORT get_d3d_viewport() const;
		const D3D11_VIEWPORT* get_d3d_viewport_ptr() const;

		mattmath::RectangleF get_rectangle() const;
		//mattmath::RectangleF get_rectangle(float minDepth, float maxDepth) const;
		mattmath::Vector2F get_position() const;
		mattmath::Vector2F get_size() const;

		//Viewport& operator=(const Viewport& viewport);
		Viewport& operator=(const DirectX::SimpleMath::Viewport& viewport);
		Viewport& operator=(const D3D11_VIEWPORT& viewport);
		Viewport& operator=(const mattmath::RectangleF& rectangle);
		Viewport& operator=(const mattmath::RectangleI& rectangle);
		Viewport& operator=(const RECT& rect);
		
		bool operator==(const Viewport& other) const;
		bool operator!=(const Viewport& other) const;
	};

	struct Circle : public Shape
	{
		mattmath::Vector2F center = mattmath::Vector2F::ZERO;
		float radius = 0.0f;

		Circle() = default;
		Circle(const Circle&) = default;
		Circle(const mattmath::Vector2F& center, float radius);
		Circle(const DirectX::SimpleMath::Vector2& center, float radius);
		Circle(float x, float y, float radius);

		mattmath::RectangleF get_bounding_box() const override;
		ShapeType get_shape_type() const override;
		void offset(const mattmath::Vector2F& amount) override;
		std::unique_ptr<Shape> clone() const override;
		std::vector<Segment> get_edges() const override;
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

		mattmath::Vector2F get_center() const override;
	};
	struct Triangle : public Shape
	{
		Vector2F points[3] = { Vector2F::ZERO, Vector2F::ZERO, Vector2F::ZERO };

		Triangle() = default;
		Triangle(const Triangle&) = default;
		Triangle(const mattmath::Vector2F& point0,
			const mattmath::Vector2F& point1,
			const mattmath::Vector2F& point2);
		Triangle(const DirectX::SimpleMath::Vector2& point0,
			const DirectX::SimpleMath::Vector2& point1,
			const DirectX::SimpleMath::Vector2& point2);
		Triangle(float x0, float y0, float x1, float y1, float x2, float y2);

		mattmath::RectangleF get_bounding_box() const override;
		ShapeType get_shape_type() const override;
		void offset(const mattmath::Vector2F& amount) override;
		std::unique_ptr<Shape> clone() const override;
		void inflate(float amount) override;

		const Vector2F& get_point_0() const;
		const Vector2F& get_point_1() const;
		const Vector2F& get_point_2() const;
		std::vector<Vector2F> get_points() const;

		Segment get_edge_0() const;
		Segment get_edge_1() const;
		Segment get_edge_2() const;
		std::vector<Segment> get_edges() const override;

		float get_angle_0() const;
		float get_angle_1() const;
		float get_angle_2() const;
		std::vector<float> get_angles() const;

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

		mattmath::Vector2F get_center() const override;

		float calculate_gradient(int edge) const;
	};
	struct TriangleRightAxisAligned : public Triangle
	{
		TriangleRightAxisAligned() = default;
		TriangleRightAxisAligned(const TriangleRightAxisAligned&) = default;
		TriangleRightAxisAligned(const mattmath::Vector2F& top,
			const mattmath::Vector2F& left, const mattmath::Vector2F& right);
		TriangleRightAxisAligned(const DirectX::SimpleMath::Vector2& top,
			const DirectX::SimpleMath::Vector2& left,
			const DirectX::SimpleMath::Vector2& right);
		TriangleRightAxisAligned(float x0, float y0, float x1, float y1,
			float x2, float y2);

		Segment get_hypotenuse() const;
		float get_hypotenuse_gradient() const;

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
		Quad(const DirectX::SimpleMath::Vector2& point1, 
			const DirectX::SimpleMath::Vector2& point2,
			const DirectX::SimpleMath::Vector2& point3,
			const DirectX::SimpleMath::Vector2& point4);

		mattmath::RectangleF get_bounding_box() const override;
		ShapeType get_shape_type() const override;
		void offset(const mattmath::Vector2F& amount) override;
		std::unique_ptr<Shape> clone() const override;
		void inflate(float amount) override;

		bool is_valid() const;

		const Point2F& get_point_0() const;
		const Point2F& get_point_1() const;
		const Point2F& get_point_2() const;
		const Point2F& get_point_3() const;
		std::vector<Point2F> get_points() const;

		void set_point_0(const Point2F& point);
		void set_point_1(const Point2F& point);
		void set_point_2(const Point2F& point);
		void set_point_3(const Point2F& point);

		Segment get_edge_0() const;
		Segment get_edge_1() const;
		Segment get_edge_2() const;
		Segment get_edge_3() const;
		std::vector<Segment> get_edges() const override;

		Triangle get_triangle_0() const;
		Triangle get_triangle_1() const;
		std::vector<Triangle> get_triangles() const;

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

		mattmath::Vector2F get_center() const override;

	private:
		Point2F points[4] = { Vector2F::ZERO, Vector2F::ZERO,
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

		mattmath::Vector2F get_direction() const;

		float get_length() const;

		mattmath::Point2F get_center() const;
		//bool intersects(const mattmath::Circle& other) const;
		//bool intersects(const mattmath::Triangle& other) const;
	};
	struct Camera
	{
		mattmath::Vector2F translation = mattmath::Vector2F::ZERO;
		float scale = 1.0f;

		Camera() = default;
		Camera(const Camera&) = default;
		Camera(const mattmath::Vector2F& translation, float scale);
		Camera(float x, float y, float scale);
		Camera(const mattmath::Viewport& viewport, float scale = 1.0f);

		bool operator==(const Camera& other) const;
		bool operator!=(const Camera& other) const;

		mattmath::RectangleF calculate_view_rectangle(
			const mattmath::RectangleF& world_rectangle) const;
		void calculate_view_rectangle(
			mattmath::RectangleF& rectangle) const;
		static mattmath::RectangleF calculate_view_rectangle(
			const mattmath::RectangleF& world_rectangle,
			const Camera& camera);
		static void calculate_view_rectangle(
			const mattmath::RectangleF& world_rectangle,
			const Camera& camera,
			mattmath::RectangleF& view_rectangle);

		static Camera calculate_intermediate_camera(
			const Camera& first, const Camera& last, float amount);

		static Camera calculate_camera_from_view_rectangle(
			const mattmath::RectangleF& view_rectangle,
			const mattmath::RectangleF& world_rectangle);

		mattmath::Vector2F calculate_view_position(
			const mattmath::Vector2F& world_position) const;
		float calculate_view_scale(float world_scale) const;

		static const Camera DEFAULT_CAMERA;
	};

	struct RectangleRotated : public Shape
	{
		RectangleRotated() = default;
		RectangleRotated(const RectangleRotated&) = default;
		RectangleRotated(const mattmath::Point2F& center,
			const mattmath::Vector2F& x_axis, const mattmath::Vector2F& y_axis,
			const mattmath::Vector2F& hw_extents);
		RectangleRotated(const mattmath::Segment& center_line, float thickness);

		RectangleF get_bounding_box() const override;
		ShapeType get_shape_type() const override;
		bool intersects(const RectangleF& rect) const override;
		bool intersects(const Circle& circle) const override;;
		bool intersects(const Triangle& triangle) const override;
		bool intersects(const Quad& quad) const override;
		bool intersects(const Segment& segment) const override;
		bool intersects(const Point2F& point) const override;
		bool intersects(const RectangleRotated& rect_rotated) const override;
		bool contains(const Point2F& point) const;
		void offset(const Vector2F& amount) override;
		std::unique_ptr<Shape> clone() const override;
		Point2F get_center() const override;
		std::vector<Segment> get_edges() const override;
		void inflate(float amount) override;

		Point2F get_x_axis() const;
		Point2F get_y_axis() const;
		Point2F get_axis(int axis) const;
		Point2F get_half_extents() const;
		float get_half_x_width() const;
		float get_half_y_width() const;
		float get_half_width(int axis) const;

		void set_center(const Point2F& center);
		void set_x_axis(const Point2F& x_axis);
		void set_y_axis(const Point2F& y_axis);
		void set_half_extents(const Point2F& hw_extents);
		void set_half_x_width(float half_x_width);
		void set_half_y_width(float half_y_width);

		Point2F get_point_0() const;
		Point2F get_point_1() const;
		Point2F get_point_2() const;
		Point2F get_point_3() const;
		const std::vector<Point2F>& get_points() const;

		Segment get_edge_0() const;
		Segment get_edge_1() const;
		Segment get_edge_2() const;
		Segment get_edge_3() const;

		Quad get_quad() const;

		RectangleF get_rectangle_rotated_to_axis() const;

		float get_angle() const;
		//RectangleRotated(const mattmath::Point2F& center,
		//	float angle, const mattmath::Vector2F& hw_extents);

		//std::vector<Point2F> get_points() const;

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
