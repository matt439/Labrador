#pragma once

#include "engine/math/segment.h"
#include "engine/math/shape.h"
#include "engine/render/colour.h"
#include "engine/render/renderer.h"

#include <cstddef>
#include <span>

namespace components
{
	inline constexpr int max_outline_segments = 1024;

	// The shape's boundary edges, in order, including zero-length edges.
	// Supports RectangleF, RectangleRotated, Triangle, Quad and Circle.
	// Circle uses 3..max_outline_segments straight chords, closed exactly on
	// its first point; radius zero produces no edges. Finite degenerate
	// polygons retain their boundary, without inventing an interior.
	//
	// Caller-owned storage: four entries for rectangles/quads, three for
	// triangles, or circle_segments for a nonzero circle. Returns the written
	// count. Throws length_error for insufficient space, invalid_argument for
	// nonfinite geometry, an invalid circle, an unsupported/lying shape tag,
	// or an invalid circle segment count. Failure leaves output unchanged.
	std::size_t outline_edges(const labrador::Shape& shape,
		std::span<labrador::Segment> output, int circle_segments);

	// COPY IT, DO NOT LINK IT. A game owns which shapes are shown, their
	// colour, thickness and circle quality; samples/components/README.md
	// describes taking the source and tests. This is not an engine debug
	// overlay or line primitive.
	class ShapeOutline
	{
	public:
		// A 1x1 white texture from the same resource table as the DrawList.
		// An unresolved default is useful before content is loaded; drawing
		// nonempty geometry needs a resolved handle. The table owns the texture.
		explicit ShapeOutline(labrador::TextureHandle white = {});

		// Draws one existing white-texture sprite quad per nonzero edge.
		// Strokes have butt ends, are centred on their boundary edges and may
		// overlap at corners. Thickness is finite, positive world units, not
		// constant screen pixels. Circle is a polygonal approximation.
		//
		// The ordinary sprite path quantizes destination edges before rotation;
		// thin/short strokes can collapse under a camera. No anti-aliasing,
		// filled geometry or special join/cap renderer is added. Tint follows
		// DrawList's premultiplied-colour contract.
		//
		// Reads shape/presentation inputs, stores no per-frame state and uses
		// fixed stack storage for geometry. Geometry and derived stroke sizes
		// are validated before the first draw; invalid values throw invalid_argument. Texture
		// lookup and renderer failures retain DrawList's own contract.
		void draw(labrador::DrawList& list, const labrador::Shape& shape,
			float thickness, const labrador::Colour& colour,
			int circle_segments) const;

	private:
		labrador::TextureHandle white_;
	};
}
