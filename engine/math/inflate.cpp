#include "engine/math/inflate.h"

#include "engine/math/scalar.h"
#include "engine/math/ericson_math.h"

#include <cmath>

namespace mattmath
{
	namespace detail
	{
		// Moves every edge of a convex polygon outward along its own normal by
		// `amount`, and puts each vertex back where its two offset edges meet.
		//
		// This is what Shape::inflate promises, and it is not what pushing the
		// vertices away from the centroid does. A vertex ray and the normal of
		// an edge meeting at that vertex differ by some angle, so displacing
		// the vertex by `amount` along the ray moves the edge outward by only
		// amount * cos(that angle) - less than asked, by a different factor at
		// every corner, and never enough. A collider that inflates by less
		// than it claims is the one direction Ericson singles out as
		// unacceptable (12.4, p.487): objects visibly interpenetrate while the
		// collision system correctly reports no touch.
		//
		// The true offset of a polygon by a disc has arcs at the corners.
		// Extending the edges to meet instead - a mitre - keeps the result a
		// polygon and always contains the true shape, so it errs outward,
		// which is the safe direction. T3: nobody will see the corner.
		void detail::inflate_convex_polygon(Vector2F* points, int count, float amount)
		{
			constexpr int MAX_POINTS = 4;
			if (count < 3 || count > MAX_POINTS)
			{
				return;
			}

			// A polygon with no interior cannot be grown outward, because it
			// has no outward: the centroid lies on the line every vertex is
			// on, so the orientation test below scores exactly zero for every
			// edge and no normal is flipped. Left to run, a collinear triangle
			// came out with two of its three original vertices OUTSIDE the
			// result - the one direction the contract on Shape::inflate says
			// this operation must never fail in.
			//
			// signed_area is the engine's existing answer to "does this
			// enclose anything", and narrow_phase.cpp uses it for the same
			// question. Leaving the shape alone matches the degenerate-edge
			// policy a few lines below: still contains the original, refuses
			// to invent geometry it cannot derive.
			const std::span<const Point2F> outline(points,
				static_cast<size_t>(count));
			if (signed_area(outline) == 0.0f)
			{
				return;
			}

			Vector2F centre = Vector2F::ZERO;
			for (int i = 0; i < count; i++)
			{
				centre += points[i];
			}
			centre /= static_cast<float>(count);

			// Each edge as an outward unit normal and the offset line's
			// constant, so a vertex is a 2x2 solve rather than a construction.
			Vector2F normals[MAX_POINTS];
			float constants[MAX_POINTS];
			for (int i = 0; i < count; i++)
			{
				const Vector2F& from = points[i];
				const Vector2F edge = points[(i + 1) % count] - from;

				Vector2F normal = Vector2F(-edge.y, edge.x).normalized();
				if (normal == Vector2F::ZERO)
				{
					// A degenerate edge has no normal to offset along. Leave
					// the polygon alone rather than invent a direction.
					return;
				}

				if (Vector2F::dot(normal, from - centre) < 0.0f)
				{
					normal = -normal;
				}

				normals[i] = normal;
				constants[i] = Vector2F::dot(from, normal) + amount;
			}

			Vector2F moved[MAX_POINTS];
			for (int i = 0; i < count; i++)
			{
				// Vertex i is shared by the edge that ends at it and the edge
				// that starts at it.
				const int previous = (i + count - 1) % count;

				const Vector2F& n0 = normals[previous];
				const Vector2F& n1 = normals[i];
				const float determinant = Vector2F::cross(n0, n1);

				// A near-zero determinant means the turn at this vertex is
				// near-nothing OR near-straight-back, and those two want
				// opposite treatment. The sign of dot(n0, n1) tells them
				// apart; testing the determinant alone conflated them.
				//
				//   dot > 0: the edges run the same way, the offset lines
				//            really are parallel and never meet, and pushing
				//            the vertex straight out along the common normal
				//            is the limit of the mitre. Correct.
				//   dot < 0: a needle. The edges double back and DO meet, at
				//            a mitre point far outside the shape. Pushing
				//            straight out there leaves the original vertex
				//            outside the result - the one failure this
				//            contract exists to prevent. For the triangle
				//            (0,0), (100, 0.001), (100, -0.001) inflated by 1
				//            the tip belongs 100,000 units out, and the
				//            straight push moved it 1 unit up.
				//
				// So solve wherever the lines actually meet, and accept that a
				// needle produces a far mitre. Containment is the invariant
				// the header states, and a caller who hands this a sliver gets
				// a shape that honours it rather than a tidier one that does
				// not. An exact zero cannot be solved at all and is
				// unreachable here: a perfect spike encloses nothing, and the
				// area guard at the top of this function has already returned.
				if (determinant == 0.0f ||
					(std::abs(determinant) < EPSILON &&
						Vector2F::dot(n0, n1) > 0.0f))
				{
					moved[i] = points[i] + n1 * amount;
					continue;
				}

				moved[i] = Vector2F(
					(constants[previous] * n1.y - constants[i] * n0.y) / determinant,
					(constants[i] * n0.x - constants[previous] * n1.x) / determinant);
			}

			for (int i = 0; i < count; i++)
			{
				points[i] = moved[i];
			}
		}
	}
}
