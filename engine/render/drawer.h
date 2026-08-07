#pragma once

#include "SpriteBatch.h"
#include "engine/render/render_resources.h"
#include "engine/render/rotation_origin.h"
#include "engine/math/matt_math.h"

namespace artattack
{
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
		// const because they are on the draw path and are pure functions of
		// their arguments - none of the three reads a member. Non-const here
		// was only an omission, but it is the kind that lets the next person
		// reach for a member to stash a partial result in, on an object every
		// render worker is inside at once.
		mattmath::RectangleI calculate_draw_rectangle(
			const mattmath::RectangleI& rec,
			const mattmath::Vector3F& camera) const;
		mattmath::RectangleI calculate_draw_rectangle(
			const mattmath::Vector2F& position,
			const mattmath::Vector2F& size,
			const mattmath::Vector3F& camera) const;
		mattmath::Vector2F calculate_sprite_origin(
			const mattmath::Vector2F& size,
			RotationOrigin origin) const;
	private:
		RenderResources* render_resources_ = nullptr;
		const float* dt_ = nullptr;
	};
}
