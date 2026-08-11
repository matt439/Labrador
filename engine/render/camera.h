#pragma once

#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
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

		// What `viewport` actually shows of the world under this camera - the
		// inverse of calculate_view_rectangle, and the only inverse this type
		// has ever offered.
		//
		// The forward transform is view = (world - translation) * scale, so
		// the world extent behind `viewport.width` pixels is
		// viewport.width / scale. Every caller that needed this wrote it by
		// hand and most of them wrote a MULTIPLY, which is the right
		// arithmetic upside down: it agrees at scale 1, is merely
		// over-inclusive above it, and collapses below it. Camera::frame
		// produces exactly the below-one case whenever a world rectangle is
		// larger than the viewport showing it - framing 6000 world units into
		// 1080 pixels gives scale 0.18, where the multiply reports a visible
		// region 31 times too small in each axis and a cull built on it throws
		// away almost everything on screen.
		//
		// Throws std::invalid_argument for a zero scale, which is not a
		// viewpoint but a division by nothing. Camera::frame cannot produce
		// one; a hand-built camera can.
		mattmath::RectangleF visible_rectangle(const Viewport& viewport) const;

		// The camera that shows all of `world_rectangle` in `viewport`.
		//
		// Both axes are honoured: the scale is whichever of the two ratios
		// fits, and the surplus on the other axis is split evenly so the
		// requested rectangle ends up centred. What this replaces,
		// `calculate_camera_from_view_rectangle`, derived the scale from the
		// widths alone and silently discarded the height it was given - so an
		// end-of-match shot authored as 3840x2160 was framed as "3840 wide,
		// and however tall the back buffer's aspect makes that". Its two
		// parameters were also named the wrong way round: the first was the
		// world rectangle and the second the view.
		//
		// Throws std::invalid_argument if either rectangle has a
		// non-positive extent - the old form divided by view_rectangle.width
		// with no guard, and a zero there is an infinite scale that poisons
		// every cull and every draw taken through the result.
		static Camera frame(const mattmath::RectangleF& world_rectangle,
			const Viewport& viewport);

		mattmath::Vector2F calculate_view_position(
			const mattmath::Vector2F& world_position) const;
		float calculate_view_scale(float world_scale) const;

		static const Camera DEFAULT_CAMERA;
	};
}
