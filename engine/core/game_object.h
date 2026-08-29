#pragma once

#include "engine/math/rectanglef.h"
#include "engine/render/camera.h"

namespace labrador
{
	// Declared, not included, and that is the point of the whole exercise.
	// Opening this header with a graphics API puts nine of the engine's
	// translation units out of a headless test's reach and makes `core | math`
	// (ARCHITECTURE's module table) false at exactly one line.
	// A reference parameter needs no definition; whoever implements draw()
	// includes engine/render/renderer.h, and core depends on math alone.
	class DrawList;
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
	class GameObject
	{
	public:
		virtual ~GameObject() = default;
		// dt arrives as a parameter rather than being read off a member. An
		// update() taking nothing makes every object that needs the frame time
		// hold a const float* to the shell's dt and be handed it at construction
		// - which puts that pointer in eleven classes, in the constructors of
		// everything that builds one, and in the builders above those. It also
		// makes the outlive-the-pointer question real, because a device loss can
		// reallocate the float behind it.
		virtual void update(float dt) = 0;
		// One draw, into one view's recording. There was a second, camera-less
		// overload; nothing ever called it through this interface, and by the
		// time it was deleted Player's copy of it had silently drifted - it had
		// stopped early-outing on death, dropped a whole sprite layer and lost
		// the facing flip.
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
		// The object's drawn extent, in world space.
		//
		// This replaced is_visible_in_viewport(view), which asked the object
		// "are you inside this box?". That phrasing can only ever be answered
		// one object at a time against one box at a time, so culling was fixed
		// at a virtual call per object per view and no amount of work inside
		// the renderer could beat it. Reporting the extent instead lets the
		// caller build an index once and query it - the same information, but
		// indexable rather than only interrogable.
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
	};
}
