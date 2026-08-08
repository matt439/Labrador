#pragma once

#include "engine/math/matt_math.h"
#include <SpriteBatch.h>

namespace artattack
{
	// Anything the level can update and draw.
	//
	// draw() is const, and that is the whole contract that makes parallel
	// rendering sound. The render workers do not own disjoint slices of the
	// object list - the parallelism axis is *views*, so every worker enters
	// draw() on the SAME object at the same time. Under that fan-out the only
	// safe draw is a pure read, and const is how the compiler holds a new
	// object to it instead of a comment nobody reads.
	//
	// What that costs an implementer: anything varying per draw - a tint, a
	// flip, a shadow offset - is computed into a local and passed down, never
	// assigned to a member first. The draw_with() overloads on TextureObject,
	// AnimationObject and TextObject exist to take those locals. Anything
	// varying per *frame* - the animation clip, the facing - is chosen in
	// update(), which runs once, on one thread.
	class IGameObject
	{
	public:
		virtual ~IGameObject() = default;
		// dt arrives as a parameter rather than being read off a member.
		// update() taking nothing meant every object that needed the frame time
		// had to hold a const float* to the shell's dt and be handed it at
		// construction - which put that pointer in eleven classes, in the
		// constructors of everything that built one, and in the builders above
		// those. It also made the object outlive-the-pointer question real:
		// device loss used to reallocate the float behind it.
		virtual void update(float dt) = 0;
		// One draw, always with a camera. There was a second, camera-less
		// overload; nothing ever called it through this interface, and by the
		// time it was deleted Player's copy of it had silently drifted - it had
		// stopped early-outing on death, dropped a whole sprite layer and lost
		// the facing flip. A camera-less draw is just Camera::DEFAULT_CAMERA,
		// which is the identity, so callers wanting screen space pass it.
		virtual void draw(DirectX::SpriteBatch* sprite_batch,
			const mattmath::Camera& camera) const = 0;
		virtual bool is_visible_in_viewport(const mattmath::RectangleF& view) const = 0;
	};
}
