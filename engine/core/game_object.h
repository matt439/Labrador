#pragma once

#include "engine/math/rectanglef.h"

namespace labrador
{
	// Declared, not included. Opening this header with a graphics API would
	// put the engine's core out of a headless test's reach and make `core |
	// math` (ARCHITECTURE's module table) false at exactly one line. A
	// reference parameter needs no definition; whoever implements draw()
	// includes engine/render/renderer.h, and core depends on math alone.
	class DrawList;

	// Anything a scene can update and draw.
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
	class GameObject
	{
	public:
		virtual ~GameObject() = default;

		// Steps the object by `dt` seconds. Called once per tick, on one thread;
		// this is where everything that changes is changed, the animation clip
		// and the facing included.
		//
		// dt arrives as a parameter rather than being read off a member. An
		// update() taking nothing makes every object that needs the frame time
		// hold a pointer to the shell's and be handed it at construction - in
		// its constructor, in the constructors of everything that builds one,
		// and in the builders above those - and then has to answer whether the
		// object outlives what the pointer points at.
		virtual void update(float dt) = 0;

		// One draw, into one view's recording.
		//
		// The camera is on the DrawList and not a parameter here
		// (engine/render/renderer.h, DrawList::set_camera). An implementation of
		// this either hands a camera straight down or calls
		// Camera::calculate_view_rectangle with it, and the caller that knows
		// which view this is is the caller that should say so - once for a
		// range of draws, not once per object per draw.
		//
		// This is the line that would make engine/core include a graphics API if
		// the parameter were anything but a reference to a declared type.
		virtual void draw(DrawList& draw_list) const = 0;

		// The object's geometry in world space, before view-pixel quantization.
		//
		// An extent rather than a question. "Are you inside this box?" can only
		// be answered one object at a time against one box at a time, which
		// fixes culling at a virtual call per object per view; reporting the
		// extent lets the caller build an index once and query it - the same
		// information, but indexable rather than only interrogable.
		//
		// It is the input a broad phase wants too: "what overlaps this view"
		// and "which pairs overlap each other" are one query against one
		// structure. CollisionObject::shape() stays the fine half of that
		// pair; this is the coarse half.
		//
		// Note what is not expressible: an object cannot report itself
		// invisible. Whether anything is drawn is a property of draw(),
		// not of the extent.
		virtual mattmath::RectangleF bounds() const = 0;

		// Conservative drawn extent for culling at this view's world units per
		// pixel (nonnegative). Ordinary geometry needs no adjustment. Drawables
		// that snap in view pixels override this: quantization happens after the camera, so
		// its world-space allowance depends on zoom. bounds() remains the
		// unsnapped geometry used by layout and collision.
		virtual mattmath::RectangleF cull_bounds(float /*units_per_pixel*/) const
		{
			return this->bounds();
		}
	};
}
