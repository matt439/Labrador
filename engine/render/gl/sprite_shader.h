#pragma once

// The only shader this backend has, and a transliteration of the only shader
// the other one has.
//
// SIDE BY SIDE WITH engine/render/d3d11/sprite.hlsl, deliberately. Both do one
// multiply-add to reach clip space, one texture fetch and one multiply, because
// every term of the pixel contract is settled on the CPU in
// engine/render/sprite_geometry.cpp before either of them runs. The two files
// differing in more than syntax would mean the contract had leaked into a
// shader, which is the failure this arrangement exists to prevent.
//
// COMPILED AT RUN TIME, WHERE THE OTHER ONE IS COMPILED AT BUILD TIME, and that
// is not a preference. GL 3.3 has no offline shader format: glShaderBinary
// needs a format the driver defines and will not accept from another driver,
// and SPIR-V arrives in 4.6 - which is a decade of hardware above the tier this
// port exists for. So the source ships, and a syntax error is a throw at device
// creation naming the compiler's own message rather than a build failure.
//
// #version 330 core, AUTHORED TO THE GLES 3.0 INTERSECTION. `in`/`out` rather
// than `attribute`/`varying`, an explicit `out` for the fragment colour rather
// than gl_FragColor, and `texture()` rather than `texture2D()` - all of which
// are what ES 3.0 wants too, so an ES variant is "#version 300 es" and a
// precision qualifier rather than a second shader.

namespace artattack
{
	const char* const SPRITE_VERTEX_SHADER = R"(#version 330 core

// Pixels to clip space, as two multiply-adds rather than a 4x4 matrix.
//
// The values are (2/width, -2/height, -1, 1): the y term is negative because
// the seam's y runs down the screen and clip space's runs up. GL's window
// origin is at the bottom left where Direct3D's is at the top, and the two
// cancel - clip y of +1 is the top of the viewport in both - so this is the
// same constant the other backend uploads, which is why RenderPixelTests can
// hold them to the same answers.
uniform vec4 pixels_to_clip;

in vec2 position;
in vec4 colour;
in vec2 texcoord;

out vec4 vertex_colour;
out vec2 vertex_texcoord;

void main()
{
	gl_Position =
		vec4(position * pixels_to_clip.xy + pixels_to_clip.zw, 0.0, 1.0);
	vertex_colour = colour;
	vertex_texcoord = texcoord;
}
)";

	const char* const SPRITE_PIXEL_SHADER = R"(#version 330 core

uniform sampler2D sprite_texture;

in vec4 vertex_colour;
in vec2 vertex_texcoord;

out vec4 fragment;

// The tint MULTIPLIES the texel rather than replacing it, which
// RenderPixelTests checks twice - a white texel under a red tint cannot tell
// the two apart, so a red texel under a green tint has to come out black.
//
// Nothing here divides by alpha or multiplies by it. The engine's blend
// equation is premultiplied (the source factor is ONE), so a texel arrives
// premultiplied and leaves premultiplied.
void main()
{
	fragment = texture(sprite_texture, vertex_texcoord) * vertex_colour;
}
)";
}
