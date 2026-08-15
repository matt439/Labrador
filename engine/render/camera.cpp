#include "engine/render/camera.h"
#include "engine/math/scalar.h"

#include <algorithm>
#include <stdexcept>

using namespace mattmath;

namespace labrador
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
	Camera Camera::frame(const RectangleF& world_rectangle,
		const Viewport& viewport)
	{
		if (world_rectangle.width <= 0.0f || world_rectangle.height <= 0.0f ||
			viewport.width <= 0.0f || viewport.height <= 0.0f)
		{
			throw std::invalid_argument(
				"Camera::frame: a world rectangle and a viewport both need a "
				"positive width and height to fit one inside the other.");
		}

		// Fit, not stretch: the smaller of the two ratios is the one that gets
		// all of `world_rectangle` on screen, and the other axis then shows
		// more than was asked for rather than less.
		const float scale = std::min(viewport.width / world_rectangle.width,
			viewport.height / world_rectangle.height);

		// Centre the slack on the axis that got the surplus, so what was asked
		// for sits in the middle of the pane rather than against one edge.
		const Vector2F shown(viewport.width / scale, viewport.height / scale);
		const Vector2F origin(
			world_rectangle.x - (shown.x - world_rectangle.width) / 2.0f,
			world_rectangle.y - (shown.y - world_rectangle.height) / 2.0f);

		return Camera(origin, scale);
	}
	RectangleF Camera::visible_rectangle(const Viewport& viewport) const
	{
		if (this->scale == 0.0f)
		{
			throw std::invalid_argument(
				"Camera::visible_rectangle: the camera's scale is zero, so it "
				"shows no region of the world");
		}

		return RectangleF(this->translation,
			Vector2F(viewport.width / this->scale,
				viewport.height / this->scale));
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
