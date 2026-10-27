#pragma once

#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/math/vector2i.h"
#include "engine/render/viewport.h"

namespace labrador
{
	// A 2D pan and zoom: what a view's world coordinates have to go through to
	// become screen coordinates. One per view, held by DrawList and applied as
	// each draw is recorded.
	//
	// A render type and not a math one, because the math library's contract
	// is that it knows nothing about drawing - and a camera is constructible from a
	// Viewport, which is a render type for the same reason.
	struct Camera
	{
		Vector2F translation = Vector2F::zero;
		float scale = 1.0f;

		Camera() = default;
		Camera(const Camera&) = default;
		Camera& operator=(const Camera&) = default;
		Camera(const Vector2F& translation, float scale);
		// constexpr and defined here so default_camera is constant-initialised
		// (camera.cpp defines it constinit), and a static in another
		// translation unit that copies it can never read it before it is set.
		constexpr Camera(float x, float y, float scale) :
			translation(x, y), scale(scale) {}
		Camera(const Viewport& viewport, float scale = 1.0f);

		bool operator==(const Camera& other) const;
		bool operator!=(const Camera& other) const;

		// Maps a world rectangle into view-local pixels: subtract translation
		// from its position, then multiply position and extent by scale.
		// The viewport's offset in the back buffer is not included.
		RectangleF calculate_view_rectangle(
			const RectangleF& world_rectangle) const;

		// Applies the same world-to-view transform in place.
		void calculate_view_rectangle(
			RectangleF& rectangle) const;

		// The same transform using the supplied camera.
		static RectangleF calculate_view_rectangle(
			const RectangleF& world_rectangle,
			const Camera& camera);

		// Writes the supplied camera's transform to `view_rectangle`.
		static void calculate_view_rectangle(
			const RectangleF& world_rectangle,
			const Camera& camera,
			RectangleF& view_rectangle);

		// Linearly interpolates translation and scale from `first` to `last`.
		// `amount` is not clamped: 0 selects first, 1 last, and values outside
		// that interval extrapolate.
		static Camera calculate_intermediate_camera(
			const Camera& first, const Camera& last, float amount);

		// What `viewport` actually shows of the world under this camera - the
		// inverse of calculate_view_rectangle for the whole pane.
		//
		// The forward transform is view = (world - translation) * scale, so
		// the world extent behind `viewport.width` pixels is
		// viewport.width / scale. Written by hand, the tempting arithmetic is
		// a MULTIPLY, which is the right arithmetic upside down: it agrees at
		// scale 1, is merely over-inclusive above it, and collapses below it.
		// Camera::frame produces exactly the below-one case whenever a world
		// rectangle is larger than the viewport showing it - framing 6000
		// world units into 1080 pixels gives scale 0.18, where the multiply
		// reports a visible region 31 times too small in each axis and a cull
		// built on it throws away almost everything on screen.
		//
		// Throws std::invalid_argument for a zero scale, which is not a
		// viewpoint but a division by nothing. Camera::frame cannot produce
		// one; a hand-built camera can.
		RectangleF visible_rectangle(const Viewport& viewport) const;

		// The world point under a back-buffer pixel seen through `viewport`
		// with this camera - the inverse of calculate_view_position. A mouse
		// position or a touch point is one: the input devices report client
		// pixels, which are the pixels a viewport is given in.
		//
		// The pane starts where every backend puts it, at the top-left of
		// viewport.pixel_rect(), so a viewport off a whole pixel answers for
		// the pixels it is drawn into. The point is the pixel's top-left
		// corner; a caller that wants its centre adds half a pixel divided by
		// the scale. Nothing is clipped to the pane: a pixel outside it
		// answers the world point the camera would put there, and which pane
		// a pointer is in is the caller's question.
		//
		// Throws std::invalid_argument for a zero scale, as visible_rectangle
		// does.
		Vector2F world_position(const Vector2I& buffer_point,
			const Viewport& viewport) const;

		// The camera that shows all of `world_rectangle` in `viewport`.
		//
		// Both axes are honoured: the scale is whichever of the two ratios
		// fits, and the surplus on the other axis is split evenly so the
		// requested rectangle ends up centred.
		//
		// Throws std::invalid_argument if either rectangle has a
		// non-positive extent: a zero there is an infinite or a zero scale,
		// which poisons every cull and every draw taken through the result.
		static Camera frame(const RectangleF& world_rectangle,
			const Viewport& viewport);

		// Maps a world point to view-local pixels: (world - translation) * scale.
		// The viewport's offset in the back buffer is not included.
		Vector2F calculate_view_position(
			const Vector2F& world_position) const;

		// Multiplies a world scale by this camera's scale.
		float calculate_view_scale(float world_scale) const;

		// The identity camera: zero translation and unit scale, suitable for
		// screen-space drawing in a view. Constant-initialised before use.
		static const Camera default_camera;
	};
}
