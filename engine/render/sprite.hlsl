// The only shader this engine has, and it is not a backend's.
//
// IT LIVES HERE RATHER THAN IN engine/render/d3d11/, WHICH IS WHERE IT USED TO
// LIVE AND WHERE ITS ONLY READER WAS. Two backends compile HLSL now, and the
// source they compile is character for character the same: the transform is a
// float4 at b0 whichever way a backend binds it, the texture is t0, the sampler
// is s0, and every decision that shows on screen was settled on the CPU before
// either of them saw a vertex. A second copy of this file under d3d12/ would be
// a generated artifact by another name - a file that can disagree with its
// source and have nothing notice, which is the failure
// cmake/compile_shaders.cmake refuses to check bytecode in to avoid.
//
// WHAT A BACKEND DOES OWN IS THE PROFILE AND THE BINDING, and both are in
// engine/CMakeLists.txt where the backend is chosen: D3D11 compiles this at
// vs_4_0_level_9_1 and ps_4_0_level_9_1 and gives the vertex shader its b0
// through a constant buffer; D3D12 compiles it at 5_1 and gives it the same b0
// as four root constants. Neither difference reaches this file, which is the
// test that it belongs above both of them. The OpenGL backend is not a
// counter-example: GLSL is a different language, not a different profile, and
// render/gl/sprite_shader.h says what that costs.
//
// IT DOES ALMOST NOTHING, AND THAT IS THE DESIGN. Every term of the pixel
// contract is settled on the CPU in engine/render/sprite_geometry.cpp, which
// hands over four corners already in view pixels with their texture
// coordinates and tint resolved. What is left for the GPU is one multiply-add
// to reach clip space, one texture fetch, and one multiply - so a second
// backend's shader is a transliteration of this file rather than a
// reimplementation of anything.
//
// COMPILED AT BUILD TIME, at the lowest profile each backend accepts:
// 4_0_level_9_1 for D3D11, which are the lowest profiles that exist, and 5_1
// for D3D12, which has no shader model below 5. Both cost nothing to ask for:
// there are no loops, no branches and no integer arithmetic here, so a later
// profile would buy the shader nothing and the two produce the same arithmetic
// - which tests/render/golden/ is what actually checks.
//
// IT IS NOT WHAT LETS THE ENGINE RUN ON A 9.1 MACHINE, AND THIS LINE USED TO
// SAY IT WAS. The D3D11 backend's DeviceResources defaults minFeatureLevel to
// D3D_FEATURE_LEVEL_10_0 and its backend.h takes the default, so the 9_3, 9_2
// and 9_1 entries are truncated out of the level array before it is ever
// requested - engine/render/d3d11/device_resources.h states that outright. The
// floor is 10.0 and the shader profile is below it, which is the safe direction
// to be wrong in and still worth not claiming the reverse. The D3D12 backend's
// floor is 11_0, because that API has no level below it.

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
