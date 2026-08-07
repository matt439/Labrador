#ifndef ERICSONMATH_H
#define ERICSONMATH_H

// Real-Time Collision Detection
// Christer Ericson
// 2005

#include "engine/math/matt_math.h"
#include <cmath>

namespace EricsonMath
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

	constexpr float EPSILON = 0.000001f;

	bool test_AABB_AABB(const MattMath::RectangleF& a,
		const MattMath::RectangleF& b);

	bool test_circle_circle(const MattMath::Circle& a,
		const MattMath::Circle& b);

	MattMath::Point2F closest_pt_point_triangle(
		const MattMath::Point2F& p,
		const MattMath::Point2F& a, const MattMath::Point2F& b,
		const MattMath::Point2F& c);

	float clamp(float n, float min, float max);

	bool test_circle_AABB(const MattMath::Circle& s,
		const MattMath::RectangleF& b);

	bool test_circle_AABB(const MattMath::Circle& s,
		const MattMath::RectangleF& b, MattMath::Point2F& p);

	float sq_dist_point_AABB(const MattMath::Point2F& p,
		const MattMath::RectangleF& b);

	void closest_pt_point_AABB(const MattMath::Point2F& p,
		const MattMath::RectangleF& b, MattMath::Point2F& q);

	//static float sq_dist_point_AABB(const MattMath::Point2F& p,
	//	const MattMath::RectangleF& b);

	bool test_circle_triangle(const MattMath::Circle& s,
		const MattMath::Point2F& a, const MattMath::Point2F& b,
		const MattMath::Point2F& c, MattMath::Point2F& p);

	//static bool test_triangle_AABB(const MattMath::Point2F& v0,
	//	const MattMath::Point2F& v1, const MattMath::Point2F& v2,
	//	const MattMath::RectangleF& b);

	bool intersect_moving_AABB_AABB(const MattMath::AABB& a,
		const MattMath::AABB& b,
		const MattMath::Vector2F& va,
		const MattMath::Vector2F& vb,
		float& tfirst, float& tlast);

	bool test_segment_AABB(const MattMath::Point2F& p0,
		const MattMath::Point2F p1, const MattMath::AABB& b);

	float signed_2D_tri_area(const MattMath::Point2F& a,
		const MattMath::Point2F& b, const MattMath::Point2F& c);

	bool test_2D_segment_segment(const MattMath::Point2F& a,
		const MattMath::Point2F& b, const MattMath::Point2F& c,
		const MattMath::Point2F& d, float& t, MattMath::Point2F& p);

	void barycentric(const MattMath::Point2F& a,
		const MattMath::Point2F& b, const MattMath::Point2F& c,
		const MattMath::Point2F& p, float& u, float& v, float& w);

	bool test_point_triangle(const MattMath::Point2F& p,
		const MattMath::Point2F& a, const MattMath::Point2F& b,
		const MattMath::Point2F& c);

	void closest_pt_point_segment(const MattMath::Point2F& c, const MattMath::Point2F& a,
		const MattMath::Point2F& b, float& t, MattMath::Point2F& d);

	// Given point p, return point q on (or in) OBB b, closest to p
	void closest_pt_point_OBB(const MattMath::Point2F& p, const MattMath::OBB& b,
		MattMath::Point2F& q);

}
#endif // !ERICSONMATH_H
