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
		// one failure a geometry simplifier must never have. Displacing each
		// vertex away from the centroid by `amount` is exactly that bug: it
		// moves the adjacent edges out by only
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
		//   edge normal can be oriented - and inventing one moves two of a
		//   collinear triangle's three vertices outside the result. Unchanged
		//   still contains the original; a guess does not. A single
		//   degenerate edge leaves the polygon unchanged in the same way.
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
		// No clone(). Every shape is copied by its own copy constructor, which
		// is what a value does (T11).
		//
		// No virtual edges(). A shape's edges are three or four segments known
		// at compile time, and a virtual returning them would have to return a
		// heap-allocated std::vector per call so that every shape could answer
		// it. Nothing asks for them through a Shape&: the callers - the
		// intersection routines in intersects.h among them - already hold the
		// concrete type. So each shape declares its own edges() returning
		// std::array of the right length, and Circle, which has no edges,
		// declares none at all.
		//
		// No virtual intersects() table, which is the same mistake at seven
		// times the size and would make this header declare eleven types before
		// it defined one. Seven pure virtuals - one per shape a shape can be
		// asked about - would make every concrete type name every other at
		// declaration time, so nothing here could be filed apart from anything
		// else, and every override would be a one-line forward to a free
		// predicate in intersects.h, which holds the body, the contract and the
		// documented degenerate cases. Choosing an overload from a Shape& means
		// recovering the concrete type with dynamic_cast: a downcast per query
		// to reach a function whose name the caller already knew (T8). The one
		// member predicate is RectangleF::intersects(const RectangleF&),
		// because box against box is what the broad phase and the scene's view
		// cull ask.
		//
		// No contains(const Point2F&) either: the *_point_intersect predicates
		// in intersects.h are that question. RectangleF::contains(const
		// RectangleF&) asks a different one, and RectangleI is not a Shape.
	};
}
