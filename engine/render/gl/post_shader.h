#pragma once

// The post stage's shaders for this backend: a transliteration of
// engine/render/post_frame.hlsli and engine/render/materials/identity.hlsl,
// under the rules sprite_shader.h states for the sprite pair. No #version
// line, because the context's prelude supplies it; written to the GLES 3.0
// intersection, so it is the same text under "#version 330 core" and under
// "#version 300 es" with highp floats and samplers; each body starts with a
// newline so the prelude may end without one.
//
// WHERE THE TWO LANGUAGES DIFFER IN WHAT THEY CAN SAY, and nowhere else:
//
// - The cbuffer is one uniform array of eleven vec4s, post_frame, filled by
//   one glUniform4fv; output_size, scene_size and frame_terms name its first
//   three elements and material_value(i) the rest, so a material reads the
//   same names it reads in HLSL.
// - A GL texture unit pairs one texture with one sampler, so the frame's two
//   samplers over input_0 are two units holding the same texture: unit 0
//   with the point sampler (input_0_point) and unit 1 with the linear one
//   (input_0_linear).
// - `input` is a reserved word in GLSL, so the material's parameter is
//   frame_input.
//
// The fragment frame is compiled with a material appended after it, as the
// HLSL material includes the frame: post_frame_pixel_shader declares
// material() and calls it, and a material constant defines it.

namespace labrador
{
	const char* const post_vertex_shader = R"(

out vec2 post_uv;

// One triangle covering the output, from the vertex index alone: uv (0, 0),
// (2, 0) and (0, 2), so the visible square is uv 0..1 with no seam. Clip y
// is Direct3D's convention, and GL's bottom-left window origin cancels it, as
// the sprite shader's negative y scale does: uv (0, 0) is the top left.
void main()
{
	int vertex_index = gl_VertexID;
	vec2 corner = vec2(float((vertex_index << 1) & 2), float(vertex_index & 2));
	post_uv = corner;
	gl_Position = vec4(corner.x * 2.0 - 1.0, 1.0 - corner.y * 2.0, 0.0, 1.0);
}
)";

	const char* const post_frame_pixel_shader = R"(

// 0: output_size, xy output pixels and zw their reciprocals; 1: scene_size,
// likewise for the scene; 2: frame_terms, x the caller's seconds, y the frame
// index, z the v sign of every input; 3..10: the material's parameters.
uniform vec4 post_frame[11];

#define output_size post_frame[0]
#define scene_size post_frame[1]
#define frame_terms post_frame[2]

uniform sampler2D input_0_point;
uniform sampler2D input_0_linear;

struct MaterialInput
{
	vec2 uv;             // 0..1 across the output, (0, 0) at the top left
	vec2 output_pixel;   // uv * output_size.xy: top-down output pixels
};

vec4 material_value(int index) { return post_frame[3 + index]; }

// Top-down uv to the texture's own v. This backend draws the scene as it
// draws framebuffer 0, bottom row first, and says so with frame_terms.z = -1;
// a material never sees it.
vec2 input_uv(vec2 uv) { return vec2(uv.x, 0.5 + (uv.y - 0.5) * frame_terms.z); }

vec4 scene_point(vec2 uv) { return textureLod(input_0_point, input_uv(uv), 0.0); }
vec4 scene_linear(vec2 uv) { return textureLod(input_0_linear, input_uv(uv), 0.0); }

vec4 material(MaterialInput frame_input);

in vec2 post_uv;

out vec4 fragment;

void main()
{
	MaterialInput frame_input;
	frame_input.uv = post_uv;
	frame_input.output_pixel = post_uv * output_size.xy;
	fragment = material(frame_input);
}
)";

	// The identity material: the scene, texel for texel. Every golden image
	// must come back byte for byte through it (docs/effects.md#10).
	const char* const post_identity_material = R"(

vec4 material(MaterialInput frame_input)
{
	return scene_point(frame_input.uv);
}
)";
}
