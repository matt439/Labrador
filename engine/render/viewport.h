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

		Viewport& operator=(const mattmath::RectangleF& rectangle);
		Viewport& operator=(const mattmath::RectangleI& rectangle);

		bool operator==(const Viewport& other) const;
		bool operator!=(const Viewport& other) const;
	};
}
