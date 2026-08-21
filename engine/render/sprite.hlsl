// The only shader this engine has, and it is not a backend's.
//
// IT LIVES HERE RATHER THAN IN engine/render/d3d11/, WHICH IS WHERE IT USED TO
// LIVE AND WHERE ITS ONLY READER WAS. Three backends compile HLSL now, and the
// source they compile is character for character the same: the transform is a
// float4 at b0 whichever way a backend binds it, the texture is t0, the sampler
// is s0, and every decision that shows on screen was settled on the CPU before
// any of them saw a vertex. A second copy of this file under d3d12/ or vulkan/
// would be a generated artifact by another name - a file that can disagree with
// its source and have nothing notice, which is the failure
// cmake/compile_shaders.cmake refuses to check bytecode in to avoid.
//
// THE THIRD READER IS THE TEST OF THE SECOND, AND IT IS NOT A THIRD PROFILE -
// IT IS A DIFFERENT COMPILER AND A DIFFERENT INTERMEDIATE. The Vulkan backend
// compiles this file with the Vulkan SDK's dxc into SPIR-V, where the two
// Direct3D ones use fxc and get DXBC, and the source still did not move. What
// it needed instead was three register shifts on the command line, because HLSL
// has a register space per resource kind and Vulkan has one binding number per
// descriptor set - so b0, t0 and s0 would all arrive at the same slot.
// cmake/compile_shaders.cmake makes that shift and says why it belongs to the
// build rather than to this file, which is the same argument as the profile
// below, one level further out.
//
// WHAT A BACKEND DOES OWN IS THE PROFILE AND THE BINDING, and they are written
// down in two different places. The profile is in engine/CMakeLists.txt where
// the backend is chosen - D3D11 asks for vs_4_0_level_9_1 and ps_4_0_level_9_1,
// D3D12 for 5_1, Vulkan for 6_0 - along with the header each one's bytes land
// in. The binding
// is in each backend's renderer.cpp, because it is a call and not a build
// setting: D3D11 gives the vertex shader its b0 through a constant buffer,
// D3D12 gives it the same b0 as four root constants, and Vulkan as a uniform
// buffer in a descriptor set. ONE difference reaches
// this file and it is worth knowing before that sentence is read as none: the
// declaration order of VertexIn's three members is an ABI term on the Vulkan
// backend, because dxc assigns SPIR-V locations in declaration order and a
// Vulkan pipeline binds attributes by number rather than by semantic. It is
// stated again where those members are, which is where somebody would reorder
// them. The OpenGL backend is not a counter-example either way: GLSL is a
// different language, not a different profile, and render/gl/sprite_shader.h
// says what that costs.
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
// 4_0_level_9_1 for D3D11, which are the lowest profiles that exist, 5_1
// for D3D12, which has no shader model below 5, and 6_0 for Vulkan, which is
// where dxc starts. None costs anything to ask for:
// there are no loops, no branches and no integer arithmetic here, so a later
// profile would buy the shader nothing and all three produce the same
// arithmetic - which tests/render/golden/ is what actually checks, and now
// checks across two compilers as well as three profiles.
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
// down the screen and Direct3D's and GL's clip space runs up.
//
// AND THAT IS NO LONGER THE ONLY LINE WHERE IT IS DECIDED, which it used to
// claim. Vulkan's clip space runs y DOWN, so this term and that one would
// cancel and every frame would be upside down - the Vulkan backend answers it
// by handing the rasteriser a negative viewport height, which flips y a second
// time (engine/render/vulkan/renderer.cpp, submit). Where a pane sits in the
// buffer is the one term renderer.h leaves to a backend, and all three answers
// to it are outside this file. What is decided HERE is the term that is the
// same for all of them: the seam's y runs down.
cbuffer ViewportTransform : register(b0)
{
	float4 pixels_to_clip;
};

Texture2D<float4> sprite_texture : register(t0);
SamplerState sprite_sampler : register(s0);

// AND THE ORDER OF THESE THREE IS AN ABI TERM ON ONE OF THE FIVE BACKENDS,
// which is the one place a backend difference does reach this file. dxc assigns
// SPIR-V input locations in declaration order and there is no semantic in the
// Vulkan pipeline's attribute descriptions to bind by, so
// engine/render/vulkan/renderer.cpp gives location 0 the offsetof(position), 1
// the colour and 2 the texcoord and would go on doing so if these three were
// reordered. The two Direct3D backends bind by semantic name and would not
// notice, and the GL one does not read this file at all. Reordering these
// members means changing that array too.
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
