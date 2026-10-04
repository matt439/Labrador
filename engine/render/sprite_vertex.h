#pragma once

#include "engine/math/vector2f.h"
#include "engine/render/colour.h"

namespace labrador
{
	// One corner of one sprite, in the form a vertex buffer wants it.
	//
	// THE FIELD SET IS THE LAYOUT; THE DECLARATION ORDER IS NOT, EXCEPT WHERE A
	// BACKEND BINDS BY LOCATION. Every backend with a vertex buffer builds its
	// offsets from offsetof, but they bind by three different things: by
	// semantic on the two Direct3D ones, by name on gl, and BY LOCATION NUMBER
	// on vulkan - where dxc assigns SPIR-V locations in declaration order, so
	// reordering these three fields silently renumbers what the pipeline binds.
	// engine/render/sprite.hlsl says the same thing about VertexIn and is the
	// other end of the same ABI. Reorder these and the three fields still
	// follow correctly on d3d11, d3d12 and gl, and swap on vulkan. What is
	// load-bearing is which fields exist, what types they are, and that this is
	// one interleaved struct rather than three parallel streams.
	//
	// NO DEPTH, AND THAT IS A DECISION RATHER THAN AN OMISSION. The seam takes
	// a layer_depth, and a float3 position could carry it as z, but nothing
	// would read it: there is no depth buffer
	// (engine/render/d3d11/device_resources.h says why it has none, and no
	// other backend makes one) and no sort mode that consults it, which
	// RenderPixelTests pins - "layer_depth does not order draws, call order
	// does". The value would be written into every vertex of every sprite and
	// then ignored by the rasteriser. Four bytes a vertex is not the point; the
	// point is that a backend reading this struct should not have to wonder
	// what a third float means.
	struct SpriteVertex
	{
		// In view pixels: x right, y down, origin at the viewport's top left.
		// A backend's own clip space is its own business and is where the
		// conversion happens.
		mattmath::Vector2F position;

		// Multiplied with the sampled texel, which is what makes the tint a
		// tint (RenderPixelTests: "the tint multiplies the texel").
		Colour colour;

		// 0..1 across the whole texture, not the source rectangle.
		mattmath::Vector2F texcoord;
	};
}
