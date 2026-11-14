// The CRT material (docs/effects.md#7). Its values come from
// crt_values (engine/render/post_effect.h):
//
// material_value(0): screen region in output uv: x, y, width, height
// material_value(1): curvature, scanlines per screen height, scanline
//                    strength, mask strength
// material_value(2): vignette, aberration in output pixels at the edge,
//                    brightness gain, unused
#include "../post_frame.hlsli"

// centred runs -1..1 across the screen. post_effect.cpp's barrel_scale is the
// other copy, for pointer mapping and tests.
float2 barrel(float2 centred, float curvature)
{
	return centred * (1.0 + curvature * dot(centred, centred));
}

// Every tap stays between the first and last texel centres inside the
// screen, so neither the fringe nor the bilinear footprint reaches what is
// drawn around it, even when the screen's edge is not on a whole pixel.
float2 clamp_to_region(float2 uv, float4 region)
{
	const float2 low_pixel = region.xy * scene_size.xy;
	const float2 high_pixel = (region.xy + region.zw) * scene_size.xy;
	const float2 first_centre = ceil(low_pixel - 0.5) + 0.5;
	const float2 last_centre = floor(high_pixel - 0.5) + 0.5;
	return clamp(uv, first_centre * scene_size.zw, last_centre * scene_size.zw);
}

// An aperture grille three output pixels wide: R, G, B columns.
float3 shadow_mask(float2 output_pixel, float strength)
{
	const float phase = frac(output_pixel.x / 3.0) * 3.0;
	const float3 grille = float3(phase < 1.0, phase >= 1.0 && phase < 2.0,
		phase >= 2.0);
	return lerp(float3(1.0, 1.0, 1.0), grille * 1.5 + 0.25, strength);
}

// The picture inside the screen, through the tube. Only called for a point
// inside the screen and inside the tube's edge.
float3 through_tube(MaterialInput input, float2 curved, float4 region,
	float4 shape, float4 finish)
{
	const float2 source = region.xy + (curved * 0.5 + 0.5) * region.zw;

	// Red and blue sampled apart along the radius, more toward the edge: red
	// from further out, so the red image sits nearer the centre and the blue
	// one further out.
	const float2 fringe = curved * finish.y * output_size.zw;
	float3 colour;
	colour.r = scene_linear(clamp_to_region(source + fringe, region)).r;
	colour.g = scene_linear(clamp_to_region(source, region)).g;
	colour.b = scene_linear(clamp_to_region(source - fringe, region)).b;

	// Scanlines follow the curved picture, a fixed count per screen height,
	// and the beam widens with brightness.
	const float brightness = dot(colour, float3(0.299, 0.587, 0.114));
	const float row = frac((source.y - region.y) / region.w * shape.y) - 0.5;
	const float beam_width = lerp(0.15, 0.45, brightness);
	const float beam = exp(-0.5 * (row * row) / (beam_width * beam_width));
	colour *= lerp(1.0, beam * 1.25, shape.z);

	// The mask sits on the glass, so it is keyed to output pixels.
	colour *= shadow_mask(input.output_pixel, shape.w);

	// Darker toward the curved edge.
	colour *= saturate(1.0 - finish.x * 0.5 * dot(curved, curved));

	return saturate(colour * finish.z);
}

// ONE RETURN, because fxc warns (X4000) on an early one in a function whose
// result it must then prove initialised.
float4 material(MaterialInput input)
{
	const float4 region = material_value(0);
	const float4 shape = material_value(1);
	const float4 finish = material_value(2);

	// Outside the screen, the pixel passes through untouched; past the
	// tube's edge it is black; otherwise the picture through the tube.
	const float2 local = (input.uv - region.xy) / region.zw;
	const float2 curved = barrel(local * 2.0 - 1.0, shape.x);
	const bool outside = any(local < 0.0) || any(local > 1.0);
	const bool past_tube = any(abs(curved) > 1.0);

	float4 result = float4(0.0, 0.0, 0.0, 1.0);
	if (outside)
	{
		result = scene_point(input.uv);
	}
	else if (!past_tube)
	{
		result = float4(through_tube(input, curved, region, shape, finish), 1.0);
	}
	return result;
}
