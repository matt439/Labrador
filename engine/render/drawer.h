#pragma once

#include "SpriteBatch.h"
#include "engine/render/render_resources.h"
#include "engine/render/rotation_origin.h"
#include "engine/math/matt_math.h"

class Drawer
{
public:
	Drawer(RenderResources* render_resources,
		const float* dt);
	void set_render_resources(RenderResources* render_resources);
	void set_dt(const float* dt);
protected:
	RenderResources* render_resources() const;
	float dt() const;
	mattmath::RectangleI calculate_draw_rectangle(
		const mattmath::RectangleI& rec,
		const mattmath::Vector3F& camera);
	mattmath::RectangleI calculate_draw_rectangle(
		const mattmath::Vector2F& position,
		const mattmath::Vector2F& size,
		const mattmath::Vector3F& camera);
	mattmath::Vector2F calculate_sprite_origin(
		const mattmath::Vector2F& size,
		RotationOrigin origin);
private:
	RenderResources* render_resources_ = nullptr;
	const float* dt_ = nullptr;
};
