#include "engine/render/camera.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace labrador
{
	namespace
	{
		// True for a finite number greater than zero. Written as a positive
		// test so a NaN, which fails every comparison, is refused with zero.
		bool positive_finite(float value)
		{
			return value > 0.0f && std::isfinite(value);
		}
	}

	Camera::Camera(const Vector2F& translation, float scale) :
		translation(translation), scale(scale)
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
		// A const copy, so overload resolution picks the returning form
		// rather than this one.
		const RectangleF world_rectangle = rectangle;
		rectangle = this->calculate_view_rectangle(world_rectangle);
	}
	Camera Camera::frame(const RectangleF& world_rectangle,
		const Viewport& viewport)
	{
		if (!std::isfinite(world_rectangle.x) ||
			!std::isfinite(world_rectangle.y) ||
			!positive_finite(world_rectangle.width) ||
			!positive_finite(world_rectangle.height) ||
			!positive_finite(viewport.width) ||
			!positive_finite(viewport.height))
		{
			throw std::invalid_argument(
				"Camera::frame: a world rectangle needs a finite position, and "
				"it and a viewport both need a finite, positive width and "
				"height to fit one inside the other.");
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

		// Finite, positive inputs can still divide out of float's range: a
		// world far larger than the pane underflows the scale to zero, and
		// one far smaller overflows it to infinity.
		if (!positive_finite(scale) || !std::isfinite(origin.x) ||
			!std::isfinite(origin.y))
		{
			throw std::invalid_argument(
				"Camera::frame: the world rectangle and the viewport differ in "
				"size by more than a float scale can represent.");
		}

		return Camera(origin, scale);
	}
	RectangleF Camera::visible_rectangle(const Viewport& viewport) const
	{
		if (this->scale == 0.0f || !std::isfinite(this->scale))
		{
			throw std::invalid_argument(
				"Camera::visible_rectangle: the camera's scale is zero or not "
				"finite, so it shows no region of the world");
		}

		return RectangleF(this->translation,
			Vector2F(viewport.width / this->scale,
				viewport.height / this->scale));
	}
	Vector2F Camera::world_position(const Vector2I& buffer_point,
		const Viewport& viewport) const
	{
		if (this->scale == 0.0f || !std::isfinite(this->scale))
		{
			throw std::invalid_argument(
				"Camera::world_position: the camera's scale is zero or not "
				"finite, so no world point is under any pixel");
		}

		const RectangleI pane = viewport.pixel_rect();
		return Vector2F(
			static_cast<float>(buffer_point.x - pane.x) / this->scale +
				this->translation.x,
			static_cast<float>(buffer_point.y - pane.y) / this->scale +
				this->translation.y);
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
	// Literals rather than Vector2F::zero, because a const object is not a
	// constant expression even when it was constant-initialised, and this
	// definition has to be one to say constinit.
	constinit const Camera Camera::default_camera = { 0.0f, 0.0f, 1.0f };
}
