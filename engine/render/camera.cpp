#include "engine/render/camera.h"

using namespace mattmath;

namespace artattack
{
	Camera::Camera(const Vector2F& translation, float scale) :
		translation(translation), scale(scale)
	{
	}
	Camera::Camera(float x, float y, float scale) :
		translation(x, y), scale(scale)
	{
	}
	Camera::Camera(const Viewport& viewport, float scale)
	{
		this->translation.x = viewport.x;
		this->translation.y = viewport.y;
		this->scale = scale;
	}

	bool Camera::operator==(const Camera& other) const
	{
		return this->translation == other.translation &&
			this->scale == other.scale;
	}
	bool Camera::operator!=(const Camera& other) const
	{
		return !(*this == other);
	}
	RectangleF Camera::calculate_view_rectangle(
		const RectangleF& world_rectangle) const
	{
		return RectangleF((world_rectangle.x - this->translation.x) * this->scale,
			(world_rectangle.y - this->translation.y) * this->scale,
			world_rectangle.width * this->scale,
			world_rectangle.height * this->scale);
	}
	void Camera::calculate_view_rectangle(RectangleF& rectangle) const
	{
		rectangle.x = (rectangle.x - this->translation.x) * this->scale;
		rectangle.y = (rectangle.y - this->translation.y) * this->scale;
		rectangle.width *= this->scale;
		rectangle.height *= this->scale;

	}
	RectangleF Camera::calculate_view_rectangle(
		const RectangleF& world_rectangle,
		const Camera& camera)
	{
		return RectangleF((world_rectangle.x - camera.translation.x) * camera.scale,
			(world_rectangle.y - camera.translation.y) * camera.scale,
			world_rectangle.width * camera.scale,
			world_rectangle.height * camera.scale);
	}
	void Camera::calculate_view_rectangle(
		const RectangleF& world_rectangle,
		const Camera& camera,
		RectangleF& view_rectangle)
	{
		view_rectangle.x = (world_rectangle.x - camera.translation.x) * camera.scale;
		view_rectangle.y = (world_rectangle.y - camera.translation.y) * camera.scale;
		view_rectangle.width = world_rectangle.width * camera.scale;
		view_rectangle.height = world_rectangle.height * camera.scale;
	}
	Camera Camera::calculate_intermediate_camera(const Camera& first, const Camera& last, float amount)
	{
		return Camera(Vector2F::lerp(first.translation, last.translation, amount),
			lerp(first.scale, last.scale, amount));
	}
	Camera Camera::calculate_camera_from_view_rectangle(
		const RectangleF& view_rectangle,
		const RectangleF& world_rectangle)
	{
		//return Camera(world_rectangle.x - view_rectangle.x,
		//	world_rectangle.y - view_rectangle.y,
		//	world_rectangle.width / view_rectangle.width);


		return Camera(view_rectangle.x - world_rectangle.x,
			view_rectangle.y - world_rectangle.y,
			world_rectangle.width / view_rectangle.width);

	}
	Vector2F Camera::calculate_view_position(
		const Vector2F& world_position) const
	{
		return Vector2F((world_position.x - this->translation.x) * this->scale,
			(world_position.y - this->translation.y) * this->scale);
	}
	float Camera::calculate_view_scale(float world_scale) const
	{
		return world_scale * this->scale;
	}
	const Camera Camera::DEFAULT_CAMERA = { Vector2F::ZERO, 1.0f };
}
