#pragma once

#include "engine/math/shape_type.h"
#include "engine/math/vector2f.h"

namespace mattmath
{
	struct RectangleF;

	struct Shape
	{
		virtual ~Shape() = default;
		virtual RectangleF bounding_box() const = 0;
		virtual ShapeType shape_type() const = 0;
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
		// Two consequences of holding containment above tidiness, both stated
		// because the arithmetic will otherwise look wrong to the next reader:
		//
		//   A polygon with no interior is left EXACTLY AS IT WAS. Collinear or
		//   coincident vertices give a shape with no outward direction to grow
		//   along - the centroid is on the same line as every vertex, so no
		//   edge normal can be oriented - and inventing one moved two of a
		//   collinear triangle's three vertices outside the result. Unchanged
		//   still contains the original; a guess did not. This matches the
		//   existing policy for a single degenerate edge.
		//
		//   A needle-sharp corner produces a FAR mitre. Two edges that double
		//   back on each other still meet, and the point where their offset
		//   lines meet can be thousands of units away for an inflation of one.
		//   That is the honest answer, and clamping it would put the original
		//   vertex outside the result, so the ceiling is accepted rather than
		//   hidden (T3). A caller inflating slivers should expect large
		//   results.
		//
		// A negative `amount` is not supported: shrinking can invert a small
		// polygon through itself, and no caller wants it.
		virtual void inflate(float amount) = 0;

		// NOT HERE: clone(), edges(), and the intersection table.
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
		//
		// intersects() was the same mistake at seven times the size, and it is
		// why this header used to declare eleven types before it defined one.
		// Seven pure virtuals - one per shape a shape could be asked about -
		// meant every concrete type had to name every other at declaration
		// time, so the forward-declaration block above existed to let Shape do
		// it, and nothing here could be filed apart from anything else. The
		// thirty-seven overrides answering them were one-line forwards to the
		// free predicates below, which held the bodies, the contracts and the
		// documented degenerate cases the whole time. Two dispatchers on top
		// recovered the concrete type with dynamic_cast to choose an overload:
		// a downcast per query, on the narrow phase's own path, to reach a
		// function whose name the caller already knew (T8).
		//
		// Counted before it went, every production call of that table was a
		// RectangleF against a RectangleF - which is why that one predicate
		// survives as a member, and why it holds a body now instead of
		// forwarding to a free function that no longer exists. The two
		// dispatchers had no caller anywhere, tests included.
		//
		// contains(const Point2F&) went with it, from all five shapes. Each
		// was a one-line forward to that shape's own intersects(Point2F)
		// override - a second spelling of a second spelling. The surviving
		// spelling is the *_point_intersect predicate below. Not affected:
		// RectangleF::contains(const RectangleF&), which asks a different
		// question and has a caller, and RectangleI's pair, which is not a
		// Shape.
	};
}
