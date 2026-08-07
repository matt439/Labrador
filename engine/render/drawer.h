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
	RenderResources* get_render_resources() const;
	float get_dt() const;
	MattMath::RectangleI calculate_draw_rectangle(
		const MattMath::RectangleI& rec,
		const MattMath::Vector3F& camera);
	MattMath::RectangleI calculate_draw_rectangle(
		const MattMath::Vector2F& position,
		const MattMath::Vector2F& size,
		const MattMath::Vector3F& camera);
	MattMath::Vector2F calculate_sprite_origin(
		const MattMath::Vector2F& size,
		rotation_origin origin);
private:
	RenderResources* render_resources_ = nullptr;
	const float* dt_ = nullptr;
};
