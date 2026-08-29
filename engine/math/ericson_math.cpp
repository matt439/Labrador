#include "engine/math/ericson_math.h"

#include <cmath>

using namespace mattmath;

namespace mattmath
{
	namespace
	{
		// Counteracts arithmetic error when a segment runs (near) parallel to a
		// coordinate axis, in test_segment_AABB alone. It is deliberately not
		// spelt EPSILON and deliberately not shared with mattmath::EPSILON: this
		// is a slab-test fudge two orders of magnitude tighter than the engine's
		// general float tolerance, and the two are not the same quantity.
		constexpr float SEGMENT_PARALLEL_EPSILON = 0.000001f;
	}


	// Returns true if sphere s intersects AABB b, false otherwise
	bool test_AABB_AABB(const RectangleF& a,
		const RectangleF& b)
	{
		Vector2F a_min = a.top_left();
		Vector2F a_max = a.bottom_right();
		Vector2F b_min = b.top_left();
		Vector2F b_max = b.bottom_right();
	
		// Stated as "overlapping on both axes", not as "not separated on
		// either". The two are the same for real numbers and are not the same
		// for NaN: every comparison against NaN is false, so the rejecting
		// form fell through to `return true` and one NaN coordinate produced a
		// box that intersected everything. This form makes the accept branch
		// something a comparison has to actually reach, so a NaN reports no
		// intersection - which is the failure a caller can survive
		// (11.2.2, pp.436-437).
		//
		// The bounds stay closed: boxes that share only an edge intersect, and
		// contacts.cpp relies on that filter being closed while narrow_phase
		// is open.
		const bool overlap_x = a_min.x <= b_max.x && b_min.x <= a_max.x;
		const bool overlap_y = a_min.y <= b_max.y && b_min.y <= a_max.y;

		return overlap_x && overlap_y;
	}

	bool test_circle_circle(const Circle& a, const Circle& b)
	{
		// Calculate squared distance between centers
		Vector2F d = a.center() - b.center();
		float dist2 = Vector2F::dot(d, d);
		// Spheres intersect if squared distance is less than squared sum of radii
		float radiusSum = a.radius() + b.radius();
		return dist2 <= radiusSum * radiusSum;
	}

	Point2F closest_pt_point_triangle(const Point2F& p,
		const Point2F& a, const Point2F& b, const Point2F& c)
	{
		// Check if P in vertex region outside A
		Vector2F ab = b - a;
		Vector2F ac = c - a;
		Vector2F ap = p - a;
		float d1 = Vector2F::dot(ab, ap);
		float d2 = Vector2F::dot(ac, ap);
		if (d1 <= 0.0f && d2 <= 0.0f) return a; // barycentric coordinates (1,0,0)
		// Check if P in vertex region outside B
		Vector2F bp = p - b;
		float d3 = Vector2F::dot(ab, bp);
		float d4 = Vector2F::dot(ac, bp);
		if (d3 >= 0.0f && d4 <= d3) return b; // barycentric coordinates (0,1,0)
		// Check if P in edge region of AB, if so return projection of P onto AB
		float vc = d1 * d4 - d3 * d2;
		if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
			// d1 - d3 is dot(ab, ab) - the squared length of ab - so it is
			// zero exactly when a and b are the same point. Unguarded, that
			// divided 0/0 and returned (NaN, NaN), which test_circle_triangle
			// hands back as the contact point on the branch where it reports
			// a hit. Triangle's constructor validates nothing, so the input is
			// reachable.
			//
			// Falling through is the fix, NOT returning a. When a and b
			// coincide the triangle is the segment a-c, and a is only the
			// closest point on it when p is beyond that end - which the vertex
			// region above has already tested for. Answering a here removes
			// the NaN and substitutes a wrong point: for a = b = (0,0),
			// c = (10,0) and p = (5,1) the answer is (5,0), and a is 5.1 away
			// from p rather than 1.
			//
			// The AC edge region below produces it, because with ab degenerate
			// vb is also zero and its guards hold. Only this branch can see a
			// zero denominator: for a == c and b == c the AB branch is reached
			// first with a positive one, and all three coincident is caught by
			// the vertex-A region at the top.
			const float ab_length_squared = d1 - d3;
			if (ab_length_squared > 0.0f)
			{
				float v = d1 / ab_length_squared;
				return a + v * ab; // barycentric coordinates (1-v,v,0)
			}
		}
		// Check if P in vertex region outside C
		Vector2F cp = p - c;
		float d5 = Vector2F::dot(ab, cp);
		float d6 = Vector2F::dot(ac, cp);
		if (d6 >= 0.0f && d5 <= d6) return c; // barycentric coordinates (0,0,1)

		// Check if P in edge region of AC, if so return projection of P onto AC
		float vb = d5 * d2 - d1 * d6;
		if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
			float w = d2 / (d2 - d6);
			return a + w * ac; // barycentric coordinates (1-w,0,w)
		}
		// Check if P in edge region of BC, if so return projection of P onto BC
		float va = d3 * d6 - d5 * d4;
		if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
			float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
			return b + w * (c - b); // barycentric coordinates (0,1-w,w)
		}
		// P inside face region. Compute Q through its barycentric coordinates (u,v,w)
		float denom = 1.0f / (va + vb + vc);
		float v = vb * denom;
		float w = vc * denom;
		return a + ab * v + ac * w; // = u*a + v*b + w*c, u = va * denom = 1.0f - v - w
	}

	// Returns true if sphere s intersects AABB b, false otherwise
	bool test_circle_AABB(const Circle& s, const RectangleF& b)
	{
		// Compute squared distance between sphere center and AABB
		float sqDist = sq_dist_point_AABB(s.center(), b);
		// Sphere and AABB intersect if the (squared) distance
		// between them is less than the (squared) sphere radius
		return sqDist <= s.radius() * s.radius();
	}

	// Returns true if sphere s intersects AABB b, false otherwise.
	// The point p on the AABB closest to the sphere center is also returned
	bool test_circle_AABB(const Circle& s,
		const RectangleF& b, Point2F& p)
	{
		// Find point p on AABB closest to sphere center
		closest_pt_point_AABB(s.center(), b, p);
		// Sphere and AABB intersect if the (squared) distance from sphere
		// center to point p is less than the (squared) sphere radius
		Vector2F v = p - s.center();
		return Vector2F::dot(v, v) <= s.radius() * s.radius();
	}

	// Computes the square distance between a point p and an AABB b
	float sq_dist_point_AABB(const Point2F& p, const RectangleF& b)
	{
		float sqDist = 0.0f;
		Vector2F b_min = b.top_left();
		Vector2F b_max = b.bottom_right();

		// For each axis count any excess distance outside box extents
		float v = p.x;
		if (v < b_min.x)
		{
			sqDist += (b_min.x - v) * (b_min.x - v);
		}
		if (v > b_max.x)
		{
			sqDist += (v - b_max.x) * (v - b_max.x);
		}

		v = p.y;
		if (v < b_min.y)
		{
			sqDist += (b_min.y - v) * (b_min.y - v);
		}
		if (v > b_max.y)
		{
			sqDist += (v - b_max.y) * (v - b_max.y);
		}
		return sqDist;
	}

	// Given point p, return the point q on or in AABB b that is closest to p
	void closest_pt_point_AABB(const Point2F& p,
		const RectangleF& b, Point2F& q)
	{
		// For each coordinate axis, if the point coordinate value is
		// outside box, clamp it to the box, else keep it as is
		Vector2F b_min = b.top_left();
		Vector2F b_max = b.bottom_right();
		float v = p.x;
		if (v < b_min.x)
		{
			v = b_min.x;
		}
		if (v > b_max.x)
		{
			v = b_max.x;
		}
		q.x = v;

		v = p.y;
		if (v < b_min.y)
		{
			v = b_min.y;
		}
		if (v > b_max.y)
		{
			v = b_max.y;
		}
		q.y = v;
	}

	// Returns true if sphere s intersects triangle ABC, false otherwise.
	// The point p on abc closest to the sphere center is also returned
	bool test_circle_triangle(const Circle& s, const Point2F& a,
		const Point2F& b, const Point2F& c, Point2F& p)
	{
		// Find point P on triangle ABC closest to sphere center
		p = closest_pt_point_triangle(s.center(), a, b, c);
		// Sphere and triangle intersect if the (squared) distance from sphere
		// center to point p is less than the (squared) sphere radius
		Vector2F v = p - s.center();
		return Vector2F::dot(v, v) <= s.radius() * s.radius();
	}





	// Test if segment specified by points p0 and p1 intersects AABB b
	bool test_segment_AABB(const Point2F& p0,
		const Point2F& p1, const AABB& b)
	{
		Vector2F b_min = b.top_left();
		Vector2F b_max = b.bottom_right();

		Point2F c = (b_min + b_max) * 0.5f; // Box center-point
		Vector2F e = b_max - c; // Box halflength extents
		Point2F m = (p0 + p1) * 0.5f; // Segment midpoint
		Vector2F d = p1 - m; // Segment halflength vector
		m = m - c; // Translate box and segment to origin

		// Stated as "overlapping on every axis", not as "not separated on
		// some axis" - the same rewrite, for the same reason, as
		// test_AABB_AABB at the top of this file. The rejecting form was three
		// comparisons that are all false against NaN, falling through to
		// `return 1`, so a segment with one poisoned endpoint coordinate hit
		// every box in the world (11.2.2, pp.436-437).
		const float adx = std::abs(d.x);
		const bool overlap_x = std::abs(m.x) <= e.x + adx;

		const float ady = std::abs(d.y);
		const bool overlap_y = std::abs(m.y) <= e.y + ady;

		// The epsilon term counteracts arithmetic error when the segment runs
		// (near) parallel to a coordinate axis (see text for detail). It
		// belongs to the cross test alone, and is still added after the two
		// axis tests have been decided, exactly as before.
		const float adx_e = adx + SEGMENT_PARALLEL_EPSILON;
		const float ady_e = ady + SEGMENT_PARALLEL_EPSILON;

		const bool overlap_cross =
			std::abs(m.x * d.y - m.y * d.x) <= e.x * ady_e + e.y * adx_e;

		// No separating axis found; segment must be overlapping AABB
		return overlap_x && overlap_y && overlap_cross;
	}

	// Returns 2 times the signed triangle area. The result is positive if
	// abc is ccw, negative if abc is cw, zero if abc is degenerate.
	float signed_2D_tri_area(const Point2F& a,
		const Point2F& b, const Point2F& c)
	{
		return (a.x - c.x) * (b.y - c.y) - (a.y - c.y) * (b.x - c.x);
	}

	bool strictly_opposite_sides(float lhs, float rhs)
	{
		return (lhs > 0.0f && rhs < 0.0f) || (lhs < 0.0f && rhs > 0.0f);
	}


	float signed_area(std::span<const Point2F> polygon)
	{
		if (polygon.size() < 3)
		{
			return 0.0f;
		}

		// Relative to the first vertex, so the products are the size of the
		// polygon and not the size of the world. The terms for the first and
		// last edges are identically zero this way, which is why the loop can
		// skip them.
		const Point2F& origin = polygon[0];

		float twice_area = 0.0f;
		for (size_t i = 1; i + 1 < polygon.size(); i++)
		{
			twice_area += Vector2F::cross(polygon[i] - origin,
				polygon[i + 1] - origin);
		}

		return twice_area * 0.5f;
	}

	bool point_in_convex_polygon(std::span<const Point2F> polygon,
		const Point2F& p)
	{
		if (polygon.size() < 3)
		{
			return false;
		}

		bool any_left = false;
		bool any_right = false;
		bool any_signed = false;

		for (size_t i = 0; i < polygon.size(); i++)
		{
			const Point2F& from = polygon[i];
			const Point2F& to = polygon[(i + 1) % polygon.size()];

			// Signed area of the edge and the point. Differences first, so
			// this stays exact at world coordinates.
			const float side = Vector2F::cross(to - from, p - from);

			// A NaN is on neither side, and it has to be rejected by a branch
			// rather than by falling past both of them. This is the same
			// argument test_AABB_AABB makes at the top of this file: every
			// comparison against NaN is false, so a test whose accept branch
			// is the fall-through accepts every poisoned input
			// (11.2.2, pp.436-437).
			if (!(side >= 0.0f || side <= 0.0f))
			{
				return false;
			}

			if (side > 0.0f)
			{
				any_left = true;
				any_signed = true;
			}
			else if (side < 0.0f)
			{
				any_right = true;
				any_signed = true;
			}

			// Sides disagree, so the point is outside this edge no matter
			// which way the caller wound the polygon.
			if (any_left && any_right)
			{
				return false;
			}
		}

		// Every edge returned exactly zero, so the polygon has no interior:
		// its vertices are coincident, or all of them are collinear. That is
		// the case signed_area already calls "encloses nothing", and this
		// function is the one other place in the file that has to agree.
		//
		// The disagreement was not academic. A default-constructed Triangle is
		// three copies of Vector2F::ZERO, so every edge vector was (0, 0),
		// every cross product was zero, and the fall-through said "inside" -
		// which made Triangle().contains(p) true for every point in the plane
		// and triangles_intersect(anything, Triangle()) true unconditionally,
		// through its containment pass.
		return any_signed;
	}

	// Test if segments ab and cd overlap. If they do, compute and return
	// intersection t value along ab and intersection position p
	bool test_2D_segment_segment(const Point2F& a,
		const Point2F& b, const Point2F& c,
		const Point2F& d, float& t, Point2F& p)
	{	
		// Sign of areas correspond to which side of ab points c and d are.
		//
		// Compared by sign rather than by multiplying the two areas together,
		// which is what the book's listing does and what the note on
		// signed_2D_tri_area forbids: two small opposite-signed areas - a
		// crossing that is nearly a graze - multiply to exactly zero, and
		// `< 0.0f` then reports no crossing. Same answer everywhere else, and
		// two fewer multiplies.
		float a1 = signed_2D_tri_area(a, b, d); // Compute winding of abd (+ or -)
		float a2 = signed_2D_tri_area(a, b, c); // To intersect, must have sign opposite of a1
		// If c and d are on different sides of ab, areas have different signs
		if (strictly_opposite_sides(a1, a2)) {
			// Compute signs for a and b with respect to segment cd
			float a3 = signed_2D_tri_area(c, d, a); // Compute winding of cda (+ or -)
			// Since area is constant a1 - a2 = a3 - a4, or a4 = a3 + a2 - a1
			// float a4 = Signed2DTriArea(c, d, b); // Must have opposite sign of a3
			float a4 = a3 + a2 - a1;
			// Points a and b on different sides of cd if areas have different signs
			if (strictly_opposite_sides(a3, a4)) {
				// Segments intersect. Find intersection point along L(t) = a + t * (b - a).
				// Given height h1 of an over cd and height h2 of b over cd,
				// t = h1 / (h1 - h2) = (b*h1/2) / (b*h1/2 - b*h2/2) = a3 / (a3 - a4),
				// where b (the base of the triangles cda and cdb, i.e., the length
				// of cd) cancels out.
				t = a3 / (a3 - a4);
				p = a + t * (b - a);
				return 1;
			}
		}
		// Segments not intersecting (or collinear)
		return 0;
	}

	// Compute barycentric coordinates (u, v, w) for
	// point p with respect to triangle (a, b, c)
	bool test_point_triangle(const Point2F& p,
		const Point2F& a, const Point2F& b, const Point2F& c)
	{
		const Point2F vertices[3] = { a, b, c };
		return point_in_convex_polygon(vertices, p);
	}

	void closest_pt_point_segment(const Point2F& c, const Point2F& a,
		const Point2F& b, float& t, Point2F& d)
	{
		const Vector2F ab = b - a;

		// The projection, with the divide by dot(ab, ab) deferred. Both clamps
		// can be decided on the undivided value, so the division happens only
		// where it is known to be safe - and only in the one case that needs
		// it (5.1.2, p.129, which supersedes the listing on the page before).
		const float projection = Vector2F::dot(c - a, ab);
		if (projection <= 0.0f)
		{
			t = 0.0f;
			d = a;
			return;
		}

		// Never negative: it is the squared length of ab. Zero exactly when
		// the segment is a point, and that case has already returned above,
		// because dot(c - a, (0, 0)) is zero and zero is not greater than
		// zero. That is the whole fix - the previous form divided first and
		// handed back (NaN, NaN) for a degenerate segment, and since NaN
		// fails every comparison, both clamps declined to catch it and the
		// caller read the result as "no intersection".
		const float length_squared = Vector2F::dot(ab, ab);
		if (projection >= length_squared)
		{
			t = 1.0f;
			d = b;
			return;
		}

		t = projection / length_squared;
		d = a + t * ab;
	}

	void closest_pt_point_OBB(const Point2F& p, const OBB& b, Point2F& q)
	{
		Vector2F d = p - b.center();
		// Start result at center of box; make steps from there
		q = b.center();
		// For each OBB axis... (2 in 2D - the textbook version this came from is 3D,
		// and OBB::axis throws for any index above 1)
		for (int i = 0; i < 2; i++)
		{
			// ...project d onto that axis to get the distance
			// along the axis of d from the box center
			float dist = Vector2F::dot(d, b.axis(i));
			// If distance farther than the box extents, clamp to the box
			if (dist > b.half_width(i)) dist = b.half_width(i);
			if (dist < -b.half_width(i)) dist = -b.half_width(i);
			// Step that distance along the axis to get world coordinate
			q += dist * b.axis(i);
		}
	}

}