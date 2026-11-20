float4 material(MaterialInput input)
{
	const float4 sampled = scene_point(input.uv);
	return lerp(sampled, sampled.gbra, material_value(1).x);
}
