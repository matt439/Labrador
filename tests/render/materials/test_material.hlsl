// Both sampling helpers must see the same top-down scene. Exact primary
// colours let the pixel case assert the channel swap with no drift allowance.
float4 material(MaterialInput input)
{
	const float4 point_sample = scene_point(input.uv);
	const float4 linear_sample = scene_linear(input.uv);
	const float4 sampled = input.output_pixel.x < output_size.x * 0.5
		? point_sample : linear_sample;
	const float4 swapped = float4(sampled.b, sampled.r, sampled.g, sampled.a);
	return lerp(sampled, swapped, material_value(0).x);
}
