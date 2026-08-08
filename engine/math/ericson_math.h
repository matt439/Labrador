#pragma once

// Real-Time Collision Detection
// Christer Ericson
// 2005

#include "engine/math/matt_math.h"
#include <cmath>

namespace mattmath
{
	//struct Vector
	//{
	//	float x = 0.0f;
	//	float y = 0.0f;

	//	Vector() = default;
	//	Vector(float x, float y);
	//	Vector(float f);

	//	Vector operator+(const Vector& rhs) const;
	//	Vector operator-(const Vector& rhs) const;
	//	Vector operator*(float rhs) const;
	//	Vector operator/(float rhs) const;
	//	Vector& operator+=(const Vector& rhs);
	//	Vector& operator-=(const Vector& rhs);
	//	Vector& operator*=(float rhs);
	//	Vector& operator/=(float rhs);
	//	Vector& operator*=(const Vector& rhs);
	//	Vector& operator/=(const Vector& rhs);

	//	Vector operator=(float rhs[2]);
	//};
	//typedef Vector Point;
	//struct AABB
	//{
	//	Point min = Point(); // Minimum x, y values of AABB
	//	Point max = Point(); // Maximum x, y values of AABB

	//	AABB() = default;
	//	AABB(Point min, Point max);
	//	AABB(float min_x, float min_y, float max_x, float max_y);

	//};
	//struct Sphere
	//{
	//	Point c = Point(); // Sphere center
	//	float r = 0.0f; // Sphere radius

	//	Sphere() = default;
	//	Sphere(Point c, float r);
	//	Sphere(float center_x, float center_y, float radius);
	//};
	//struct OBB {
	//	Point c; // OBB center point
	//	Vector u[2]; // Local x-, y-, and z-axes
	//	Vector e; // Positive halfwidth extents of OBB along each axis
	//};

	//static Vector operator*(float lhs, const Vector& rhs);


	//static int TestAABBAABB(AABB a, AABB b);
	//static int TestSphereSphere(Sphere a, Sphere b);
	//static Point ClosestPtPointTriangle(Point p, Point a, Point b, Point c);
	//static float Clamp(float n, float min, float max);
	//static int TestSphereAABB(Sphere s, AABB b);
	//static int TestSphereAABB(Sphere s, AABB b, Point& p);
	//static float SqDistPointAABB(Point p, AABB b);
	//static void ClosestPtPointAABB(Point p, AABB b, Point& q);
	//static float SqDistPointAABB(Point p, AABB b);
	//static float Dot(Vector a, Vector b);

	bool test_AABB_AABB(const mattmath::RectangleF& a,
		const mattmath::RectangleF& b);

	bool test_circle_circle(const mattmath::Circle& a,
		const mattmath::Circle& b);

	mattmath::Point2F closest_pt_point_triangle(
		const mattmath::Point2F& p,
		const mattmath::Point2F& a, const mattmath::Point2F& b,
		const mattmath::Point2F& c);

	bool test_circle_AABB(const mattmath::Circle& s,
		const mattmath::RectangleF& b);

	bool test_circle_AABB(const mattmath::Circle& s,
		const mattmath::RectangleF& b, mattmath::Point2F& p);

	float sq_dist_point_AABB(const mattmath::Point2F& p,
		const mattmath::RectangleF& b);

	void closest_pt_point_AABB(const mattmath::Point2F& p,
		const mattmath::RectangleF& b, mattmath::Point2F& q);

	//static float sq_dist_point_AABB(const mattmath::Point2F& p,
	//	const mattmath::RectangleF& b);

	bool test_circle_triangle(const mattmath::Circle& s,
		const mattmath::Point2F& a, const mattmath::Point2F& b,
		const mattmath::Point2F& c, mattmath::Point2F& p);

	//static bool test_triangle_AABB(const mattmath::Point2F& v0,
	//	const mattmath::Point2F& v1, const mattmath::Point2F& v2,
	//	const mattmath::RectangleF& b);

	bool intersect_moving_AABB_AABB(const mattmath::AABB& a,
		const mattmath::AABB& b,
		const mattmath::Vector2F& va,
		const mattmath::Vector2F& vb,
		float& tfirst, float& tlast);

	bool test_segment_AABB(const mattmath::Point2F& p0,
		const mattmath::Point2F p1, const mattmath::AABB& b);

	// Twice the signed area of triangle abc. This is the book's ORIENT2D
	// predicate, and it is exactly mattmath::Vector2F::cross(b - a, c - a) -
	// see that function for what the sign means and for the y-down caveat.
	// Written out here in the subtracted form the book uses, which needs no
	// temporaries.
	//
	// Zero means the three points are collinear. Callers wanting only the
	// orientation should compare this against zero, or compare two of these
	// results for equal sign - never multiply two of them together, which
	// squares the magnitudes and overflows to zero or infinity on
	// world-scale coordinates long before the signs are in doubt.
	float signed_2D_tri_area(const mattmath::Point2F& a,
		const mattmath::Point2F& b, const mattmath::Point2F& c);

	bool test_2D_segment_segment(const mattmath::Point2F& a,
		const mattmath::Point2F& b, const mattmath::Point2F& c,
		const mattmath::Point2F& d, float& t, mattmath::Point2F& p);

	void barycentric(const mattmath::Point2F& a,
		const mattmath::Point2F& b, const mattmath::Point2F& c,
		const mattmath::Point2F& p, float& u, float& v, float& w);

	bool test_point_triangle(const mattmath::Point2F& p,
		const mattmath::Point2F& a, const mattmath::Point2F& b,
		const mattmath::Point2F& c);

	void closest_pt_point_segment(const mattmath::Point2F& c, const mattmath::Point2F& a,
		const mattmath::Point2F& b, float& t, mattmath::Point2F& d);

	// Given point p, return point q on (or in) OBB b, closest to p
	void closest_pt_point_OBB(const mattmath::Point2F& p, const mattmath::OBB& b,
		mattmath::Point2F& q);

}
