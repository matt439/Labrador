#ifndef DRAWER_H
#define DRAWER_H

#include "SpriteBatch.h"
#include "engine/assets/resource_manager.h"
#include "engine/render/rotation_origin.h"
#include "engine/math/matt_math.h"

class Drawer
{
public:
	Drawer(ResourceManager* resource_manager,
		const float* dt);
	void set_resource_manager(ResourceManager* resource_manager);
	void set_dt(const float* dt);
protected:
	ResourceManager* get_resource_manager() const;
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
	ResourceManager* _resource_manager = nullptr;
	const float* _dt = nullptr;
};

#endif // !DRAWER_H