#pragma once

#include "engine/math/matt_math.h"
#include "engine/render/viewport.h"

namespace artattack
{
	// A 2D pan and zoom: what a view's world coordinates have to go through to
	// become screen coordinates. One per view, held by DrawList and applied as
	// each draw is recorded.
	//
	// It was a mattmath type, which put the word "camera" in a library whose
	// contract is that it knows nothing about drawing - and it is constructible
	// from a Viewport, which was the same problem twice.
	struct Camera
	{
		mattmath::Vector2F translation = mattmath::Vector2F::ZERO;
		float scale = 1.0f;

		Camera() = default;
		Camera(const Camera&) = default;
		Camera(const mattmath::Vector2F& translation, float scale);
		Camera(float x, float y, float scale);
		Camera(const Viewport& viewport, float scale = 1.0f);

		bool operator==(const Camera& other) const;
		bool operator!=(const Camera& other) const;

		mattmath::RectangleF calculate_view_rectangle(
			const mattmath::RectangleF& world_rectangle) const;
		void calculate_view_rectangle(
			mattmath::RectangleF& rectangle) const;
		static mattmath::RectangleF calculate_view_rectangle(
			const mattmath::RectangleF& world_rectangle,
			const Camera& camera);
		static void calculate_view_rectangle(
			const mattmath::RectangleF& world_rectangle,
			const Camera& camera,
			mattmath::RectangleF& view_rectangle);

		static Camera calculate_intermediate_camera(
			const Camera& first, const Camera& last, float amount);

		static Camera calculate_camera_from_view_rectangle(
			const mattmath::RectangleF& view_rectangle,
			const mattmath::RectangleF& world_rectangle);

		mattmath::Vector2F calculate_view_position(
			const mattmath::Vector2F& world_position) const;
		float calculate_view_scale(float world_scale) const;

		static const Camera DEFAULT_CAMERA;
	};
}
