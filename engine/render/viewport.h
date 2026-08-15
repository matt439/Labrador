#pragma once

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"

namespace labrador
{
	// A rectangle of the back buffer, plus a depth range, in the shape every
	// graphics API wants it. It was a mattmath type carrying a D3D11_VIEWPORT
	// conversion, a SimpleMath::Viewport conversion and a reinterpret_cast to
	// the first, in a library documented as depending on nothing. The
	// conversion left with the rest of them; the type follows it into render/,
	// which is the only module that has ever needed one.
	//
	// minDepth and maxDepth are camelCase because they are the field names
	// D3D11_VIEWPORT uses, and this is the struct that mirrors it.
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
		Viewport(const mattmath::RectangleF& rectangle,
			float minDepth = 0.0f, float maxDepth = 1.0f);
		Viewport(const mattmath::RectangleI& rectangle,
			float minDepth = 0.0f, float maxDepth = 1.0f);

		mattmath::RectangleF rectangle() const;
		mattmath::Vector2F position() const;
		mattmath::Vector2F size() const;

		// The whole pixels this viewport covers, and THE ONLY CONVERSION TO
		// THEM ANY BACKEND IS ALLOWED TO PERFORM.
		//
		// EACH EDGE, THEN THE DIFFERENCE, which is the rule
		// sprite_geometry.h's build_sprite_quad already states for a
		// destination rectangle and this is the same question about a bigger
		// one. Truncating the position and the size separately is the obvious
		// alternative and it loses rows: two panes splitting a height of 721
		// come out 360 and 360 that way, leaving the last row of the back
		// buffer covered by neither, where each-edge-then-the-difference gives
		// 360 and 361 and covers all of it.
		//
		// IT EXISTS BECAUSE A BACKEND CANNOT BE TRUSTED TO AGREE WITH ITSELF.
		// GL 3.3 core has no float glViewport - glViewportIndexedf is 4.1 - so
		// the GL backend has to make this conversion whatever the seam says.
		// It used to make it inline and then divide by the UN-truncated float
		// when it built the pixels-to-clip transform, so the rasteriser and
		// the projection disagreed about how big the viewport was and every
		// sprite in a fractional pane was scaled by the ratio between them.
		// One rectangle, computed once, feeds both.
		mattmath::RectangleI pixel_rect() const;

		Viewport& operator=(const mattmath::RectangleF& rectangle);
		Viewport& operator=(const mattmath::RectangleI& rectangle);

		bool operator==(const Viewport& other) const;
		bool operator!=(const Viewport& other) const;
	};
}
