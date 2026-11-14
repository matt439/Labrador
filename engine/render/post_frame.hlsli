// The engine's half of every post material (docs/effects.md#5).
//
// A material includes this file, declares no register, reads no SV_Position,
// and defines exactly one function: float4 material(MaterialInput input).
// Everything else - the registers, the full-screen triangle, the entry points
// and which way up an input is stored - is decided here, once.

cbuffer PostFrame : register(b0)
{
	// xy: output pixels; zw: 1 / output pixels.
	float4 output_size;
	// xy: scene pixels; zw: 1 / scene pixels.
	float4 scene_size;
	// x: the caller's seconds; y: frame index; z: v sign of every input.
	float4 frame_terms;
	// The material's parameters, in order.
	float4 material_values[8];
};

Texture2D<float4> input_0 : register(t0);   // the scene, or the previous pass
SamplerState point_sampler : register(s0);
SamplerState linear_sampler : register(s1);

struct MaterialInput
{
	float2 uv;             // 0..1 across the output, (0, 0) at the top left
	float2 output_pixel;   // uv * output_size.xy: top-down output pixels
};

float4 material_value(int index) { return material_values[index]; }

// Top-down uv to the texture's own v. A backend that stores an input bottom
// row first says so through frame_terms.z; a material never sees it.
float2 input_uv(float2 uv) { return float2(uv.x, 0.5 + (uv.y - 0.5) * frame_terms.z); }

float4 scene_point(float2 uv) { return input_0.SampleLevel(point_sampler, input_uv(uv), 0); }
float4 scene_linear(float2 uv) { return input_0.SampleLevel(linear_sampler, input_uv(uv), 0); }

struct PostVaryings
{
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

// One triangle covering the output, from the vertex index alone: uv (0, 0),
// (2, 0) and (0, 2), so the visible square is uv 0..1 with no seam.
PostVaryings post_vertex_main(uint vertex_index : SV_VertexID)
{
	PostVaryings result;
	const float2 corner = float2((vertex_index << 1) & 2, vertex_index & 2);
	result.uv = corner;
	result.position = float4(corner.x * 2.0 - 1.0, 1.0 - corner.y * 2.0, 0.0, 1.0);
	return result;
}

float4 material(MaterialInput input);

float4 post_pixel_main(PostVaryings varyings) : SV_Target
{
	MaterialInput input;
	input.uv = varyings.uv;
	input.output_pixel = varyings.uv * output_size.xy;
	return material(input);
}
