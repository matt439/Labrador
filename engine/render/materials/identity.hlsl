// The identity material: the scene, texel for texel. Every golden image must
// come back byte for byte through it (docs/effects.md#10).
#include "../post_frame.hlsli"

float4 material(MaterialInput input)
{
	return scene_point(input.uv);
}
