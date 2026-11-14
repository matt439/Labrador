#pragma once

// The post stage's shaders for this backend: a transliteration of
// engine/render/post_frame.hlsli and of each material in
// engine/render/materials/, under the rules sprite_shader.h states for the
// sprite pair. No #version line, because the context's prelude supplies it;
// written to the GLES 3.0 intersection, so it is the same text under
// "#version 330 core" and under "#version 300 es" with highp floats and
// samplers; each body starts with a newline so the prelude may end without
// one.
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
// - GLSL ES 3.00 allows const only on a constant expression, so a material's
//   locals are not const.
// - Neither dialect turns a bvec3 into a vec3, or compares vectors with < or
//   >, so a comparison becomes float(...) one channel at a time and a vector
//   test any(lessThan(...)) or any(greaterThan(...)).
// - saturate, frac and lerp are clamp(x, 0.0, 1.0), fract and mix.
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

	// The CRT material (docs/effects.md#7), engine/render/materials/crt.hlsl
	// term for term: the same arithmetic in the same order, so the two
	// languages draw the same picture. Its values come from crt_values
	// (engine/render/post_effect.h):
	//
	// material_value(0): screen region in output uv: x, y, width, height
	// material_value(1): curvature, scanlines per screen height, scanline
	//                    strength, mask strength
	// material_value(2): vignette, aberration in output pixels at the edge,
	//                    brightness gain, unused
	const char* const post_crt_material = R"(

// centred runs -1..1 across the screen. post_effect.cpp's barrel_scale is the
// other copy, for pointer mapping and tests.
vec2 barrel(vec2 centred, float curvature)
{
	return centred * (1.0 + curvature * dot(centred, centred));
}

// Every tap stays between the first and last texel centres inside the
// screen, so neither the fringe nor the bilinear footprint reaches what is
// drawn around it, even when the screen's edge is not on a whole pixel.
vec2 clamp_to_region(vec2 uv, vec4 region)
{
	vec2 low_pixel = region.xy * scene_size.xy;
	vec2 high_pixel = (region.xy + region.zw) * scene_size.xy;
	vec2 first_centre = ceil(low_pixel - 0.5) + 0.5;
	vec2 last_centre = floor(high_pixel - 0.5) + 0.5;
	return clamp(uv, first_centre * scene_size.zw, last_centre * scene_size.zw);
}

// An aperture grille three output pixels wide: R, G, B columns.
vec3 shadow_mask(vec2 output_pixel, float strength)
{
	float phase = fract(output_pixel.x / 3.0) * 3.0;
	vec3 grille = vec3(float(phase < 1.0), float(phase >= 1.0 && phase < 2.0),
		float(phase >= 2.0));
	return mix(vec3(1.0, 1.0, 1.0), grille * 1.5 + 0.25, strength);
}

// The picture inside the screen, through the tube. Only called for a point
// inside the screen and inside the tube's edge.
vec3 through_tube(MaterialInput frame_input, vec2 curved, vec4 region,
	vec4 shape, vec4 finish)
{
	vec2 source = region.xy + (curved * 0.5 + 0.5) * region.zw;

	// Red and blue sampled apart along the radius, more toward the edge: red
	// from further out, so the red image sits nearer the centre and the blue
	// one further out.
	vec2 fringe = curved * finish.y * output_size.zw;
	vec3 colour;
	colour.r = scene_linear(clamp_to_region(source + fringe, region)).r;
	colour.g = scene_linear(clamp_to_region(source, region)).g;
	colour.b = scene_linear(clamp_to_region(source - fringe, region)).b;

	// Scanlines follow the curved picture, a fixed count per screen height,
	// and the beam widens with brightness.
	float brightness = dot(colour, vec3(0.299, 0.587, 0.114));
	float row = fract((source.y - region.y) / region.w * shape.y) - 0.5;
	float beam_width = mix(0.15, 0.45, brightness);
	float beam = exp(-0.5 * (row * row) / (beam_width * beam_width));
	colour *= mix(1.0, beam * 1.25, shape.z);

	// The mask sits on the glass, so it is keyed to output pixels.
	colour *= shadow_mask(frame_input.output_pixel, shape.w);

	// Darker toward the curved edge.
	colour *= clamp(1.0 - finish.x * 0.5 * dot(curved, curved), 0.0, 1.0);

	return clamp(colour * finish.z, 0.0, 1.0);
}

// One return, as the HLSL has.
vec4 material(MaterialInput frame_input)
{
	vec4 region = material_value(0);
	vec4 shape = material_value(1);
	vec4 finish = material_value(2);

	// Outside the screen, the pixel passes through untouched; past the
	// tube's edge it is black; otherwise the picture through the tube.
	vec2 local = (frame_input.uv - region.xy) / region.zw;
	vec2 curved = barrel(local * 2.0 - 1.0, shape.x);
	bool outside = any(lessThan(local, vec2(0.0))) ||
		any(greaterThan(local, vec2(1.0)));
	bool past_tube = any(greaterThan(abs(curved), vec2(1.0)));

	vec4 result = vec4(0.0, 0.0, 0.0, 1.0);
	if (outside)
	{
		result = scene_point(frame_input.uv);
	}
	else if (!past_tube)
	{
		result = vec4(through_tube(frame_input, curved, region, shape, finish),
			1.0);
	}
	return result;
}
)";
}
