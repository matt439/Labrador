#pragma once

// Real-Time Collision Detection
// Christer Ericson
// 2005

#include "engine/math/matt_math.h"
#include <cmath>
#include <span>

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

	// The signed area enclosed by a polygon given as its vertices in order,
	// closing automatically from the last back to the first. Positive for one
	// winding and negative for the other; zero when the polygon encloses
	// nothing, which includes every degenerate case - fewer than three
	// vertices, repeated vertices, all vertices collinear.
	//
	// This is Newell's method (12.4.2) with two dimensions removed. Newell
	// derives a polygon's normal by summing a term over every edge rather than
	// taking one cross product of two edges, which is what makes it robust
	// when edges are nearly parallel. In 2D the x and y components of that sum
	// vanish identically and only the z term survives, which is twice the
	// shoelace area - so the robust formulation and the schoolbook one turn
	// out to be the same sum, and it costs one multiply-subtract per edge.
	//
	// Vertices are taken relative to the first before anything is multiplied.
	// Without that the products are the size of the world squared - near 3e7
	// out where the levels run, where consecutive floats are 2 apart - and the
	// sum must then cancel back down to the size of the polygon. The error
	// that survives is set by the coordinates, not by the shape, so it is a
	// rounding artefact on a large polygon and comparable to the answer on a
	// small one: a 13.3-unit square at (6232.75, 5408.46) comes out as 176.0
	// against a true 176.88. Working relative to a vertex makes every product
	// the size of the polygon instead.
	float signed_area(std::span<const mattmath::Point2F> polygon);

	// Whether a point lies inside a convex polygon, treating the boundary as
	// inside.
	//
	// The test is one signed area per edge: a point is inside a convex shape
	// exactly when it is on the same side of every edge (9.1, step 1). No
	// division, no allocation, no triangulation, and it terminates early on
	// the first edge that disagrees.
	//
	// Winding-agnostic by construction - it asks whether the signs agree, not
	// whether they are positive - because nothing in this library constrains
	// the order a caller lists vertices in. A point exactly on an edge or a
	// vertex produces a zero, which agrees with either sign and so counts as
	// inside.
	//
	// Convexity is a precondition and is NOT checked; a concave polygon will
	// report false for points that are genuinely inside it. Fewer than three
	// vertices encloses nothing and is always false.
	bool point_in_convex_polygon(std::span<const mattmath::Point2F> polygon,
		const mattmath::Point2F& p);

	bool test_2D_segment_segment(const mattmath::Point2F& a,
		const mattmath::Point2F& b, const mattmath::Point2F& c,
		const mattmath::Point2F& d, float& t, mattmath::Point2F& p);

	// Whether p lies in triangle abc, boundary included, for either winding.
	//
	// This is point_in_convex_polygon over three vertices, and it is written
	// that way rather than duplicated so the engine has one definition of
	// "inside" (5.4.2, pp.203-206). The barycentric form it replaces divided
	// by the triangle's determinant, which is zero for a collinear triangle -
	// and every comparison against the resulting NaN was false, so a
	// degenerate triangle reported "outside" for every point in the plane
	// rather than reporting anything a caller could act on.
	bool test_point_triangle(const mattmath::Point2F& p,
		const mattmath::Point2F& a, const mattmath::Point2F& b,
		const mattmath::Point2F& c);

	// The point d on segment ab closest to c, and the t with d = a + t(b - a).
	//
	// A degenerate segment - a and b the same point - returns that point with
	// t = 0 rather than dividing by its zero length. The division is deferred
	// until both clamps have been decided, so it runs only where the divisor
	// is known positive (5.1.2, p.129), which also makes the two clamped
	// cases cheaper than the form on the previous page.
	void closest_pt_point_segment(const mattmath::Point2F& c, const mattmath::Point2F& a,
		const mattmath::Point2F& b, float& t, mattmath::Point2F& d);

	// Given point p, return point q on (or in) OBB b, closest to p
	void closest_pt_point_OBB(const mattmath::Point2F& p, const mattmath::OBB& b,
		mattmath::Point2F& q);

}
