// The only shader this engine has.
//
// IT DOES ALMOST NOTHING, AND THAT IS THE DESIGN. Every term of the pixel
// contract is settled on the CPU in engine/render/sprite_geometry.cpp, which
// hands over four corners already in view pixels with their texture
// coordinates and tint resolved. What is left for the GPU is one multiply-add
// to reach clip space, one texture fetch, and one multiply - so a second
// backend's shader is a transliteration of this file rather than a
// reimplementation of anything.
//
// COMPILED AT BUILD TIME, at vs_4_0_level_9_1 and ps_4_0_level_9_1. Those are
// the lowest profiles that exist, and they are chosen rather than defaulted:
// device_resources.cpp accepts a device down to feature level 9.1, so a shader
// that needed 10.0 would turn a machine this engine is meant to run on into a
// device-creation failure nobody could read. Nothing here wants a later
// profile - there are no loops, no branches and no integer arithmetic.

// Pixels to clip space, as two multiply-adds rather than a 4x4 matrix.
//
// A matrix is what SpriteBatch used, and sixteen floats to express a 2D scale
// and offset is fifteen more than the arithmetic needs. The values are
// (2/width, -2/height, -1, 1): the y term is negative because the seam's y runs
// down the screen and clip space's runs up, which is the single line in this
// engine where that is decided.
cbuffer ViewportTransform : register(b0)
{
	float4 pixels_to_clip;
};

Texture2D<float4> sprite_texture : register(t0);
SamplerState sprite_sampler : register(s0);

struct VertexIn
{
	float2 position : POSITION;
	float4 colour   : COLOR;
	float2 texcoord : TEXCOORD;
};

struct PixelIn
{
	float4 position : SV_Position;
	float4 colour   : COLOR;
	float2 texcoord : TEXCOORD;
};

PixelIn vertex_main(VertexIn input)
{
	PixelIn output;
	output.position = float4(
		input.position * pixels_to_clip.xy + pixels_to_clip.zw, 0.0f, 1.0f);
	output.colour = input.colour;
	output.texcoord = input.texcoord;
	return output;
}

// The tint MULTIPLIES the texel rather than replacing it, which
// RenderPixelTests checks twice - a white texel under a red tint cannot tell
// the two apart, so a red texel under a green tint has to come out black.
//
// Nothing here divides by alpha or multiplies by it. The engine's blend
// equation is premultiplied (the source factor is ONE), so a texel arrives
// premultiplied and leaves premultiplied, and a tint's alpha scaling the
// colour is exactly what premultiplied fading is.
float4 pixel_main(PixelIn input) : SV_Target
{
	return sprite_texture.Sample(sprite_sampler, input.texcoord) *
		input.colour;
}
