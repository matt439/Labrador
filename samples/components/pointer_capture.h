#pragma once

#include "engine/input/mouse.h"
#include "engine/input/touch.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/math/vector2i.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace components
{
	// What the one pointer a page follows did this tick.
	enum class PointerPhase
	{
		// Nothing is being followed.
		none,
		// A contact began: the mouse's left button went down, or a finger.
		pressed,
		// The contact that began earlier is still down, here.
		held,
		// It ended where the player let go: a click or a tap completes here.
		released,
		// It ended without being let go - focus left the window, or the
		// platform took the touch away - so nothing it was doing completes.
		cancelled
	};

	struct PointerSample
	{
		PointerPhase phase = PointerPhase::none;

		// Client pixels, as Mouse::position and TouchPoint::position are; the
		// page turns them into its own space with the camera it drew with
		// (Camera::world_position). For released, where the contact was last
		// seen; for cancelled and none, nothing.
		labrador::Vector2I position;
	};

	// One pointer at a time, from the mouse or a touchscreen: the contact a
	// page's click, tap or drag follows from press to release.
	//
	// COPY IT, DO NOT LINK IT. This file and pointer_capture.cpp are meant to
	// be dropped into a game and edited there, as the README beside them
	// says. Everything in it is policy, which is why it is not the engine's
	// (PHILOSOPHY, UI): another game may want two fingers, or the right
	// button.
	//
	// THE RULES IT KEEPS, each one a way a menu otherwise acts twice or acts
	// on something nobody did:
	//
	// - The first contact to begin is followed until it ends. A second
	//   finger, or the mouse while a finger is down, is ignored; so is a
	//   finger already down when the first one lifts, because it never began
	//   while nothing was followed.
	// - A mouse press while any finger is down, or in the tick after one
	//   lifted, is ignored. Both engine windows already drop the mouse events
	//   a platform makes of a touch; this is the same rule kept by the page
	//   for a platform that does not.
	// - A contact that goes away without a release - the window lost focus,
	//   the platform cancelled the touch - ends cancelled, never released.
	class PointerCapture
	{
	public:
		// Reads this tick's devices, after the shell has polled them.
		PointerSample update(const labrador::Mouse& mouse,
			const labrador::Touch& touch);

		// Stops following the contact, so its release does nothing. A page
		// calls it when another input takes over, such as a key press.
		void cancel();

		bool active() const;

	private:
		enum class Owner
		{
			none,
			mouse,
			touch
		};

		Owner owner_ = Owner::none;
		uint32_t touch_id_ = 0;

		// Ticks left in which a mouse press is still too close to a finger.
		int quiet_ticks_ = 0;
	};

	// A place on the page a pointer can choose, in the page's own space.
	struct PointerTarget
	{
		labrador::RectangleF bounds;

		// A disabled target is not chosen and does not hide one beneath it.
		bool enabled = true;
	};

	// The first enabled target containing `point`, with left and top edges
	// inside and right and bottom outside, so two targets that share an edge
	// never both answer; or nothing.
	std::optional<size_t> hit_target(std::span<const PointerTarget> targets,
		const labrador::Vector2F& point);
}
