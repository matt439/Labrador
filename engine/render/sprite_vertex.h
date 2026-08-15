#pragma once

#include "engine/math/vector2f.h"
#include "engine/render/colour.h"

namespace labrador
{
	// One corner of one sprite, in the form a vertex buffer wants it.
	//
	// THE FIELDS ARE IN BUFFER ORDER AND THE STRUCT IS THE LAYOUT. A backend
	// declares its input layout from these three fields and their offsets, so
	// reordering them here silently changes what every shader reads. That is
	// the only rule about this type and it is worth stating, because it is not
	// visible from either side on its own.
	//
	// NO DEPTH, AND THAT IS A DELETION RATHER THAN AN OMISSION. DirectXTK's
	// sprite vertex carried a float3 position whose z was the layer_depth the
	// seam takes. Nothing read it: there is no depth buffer (device_resources.h
	// says why it has none) and no sort mode that consults it, which
	// RenderPixelTests pins - "layer_depth does not order draws, call order
	// does". So the value was written into every vertex of every sprite and
	// then ignored by the rasteriser. Four bytes a vertex is not the point; the
	// point is that a second backend reading this struct should not have to
	// wonder what the third float means.
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
