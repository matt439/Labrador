#pragma once

#include "engine/math/rectanglef.h"

namespace labrador
{
	// A reference needs no render definition: core depends on math alone
	// (docs/design/ARCHITECTURE.md). draw() implementations include
	// engine/render/renderer.h where they use the recording interface.
	class DrawList;

	// Anything a scene can update and draw.
	//
	// Several views may draw the same object concurrently. Drawing is a pure
	// read of shared state, with output recorded into the supplied DrawList.
	// Per-view tint, flip and offset values stay in locals and are passed to
	// draw_with() overloads instead of stored members.
	// Per-frame choices, such as animation and facing, belong in update().
	class GameObject
	{
	public:
		virtual ~GameObject() = default;

		// Steps the object by dt seconds, once per tick on one thread. Changes
		// to object state belong here rather than in draw().
		virtual void update(float dt) = 0;

		// One draw, into one view's recording.
		//
		// The caller sets the view's camera and viewport on draw_list
		// (engine/render/renderer.h). Read camera() and viewport() for view-
		// dependent drawing and do not store either. The list is valid until
		// the next Renderer::submit(). Calls for other views may run concurrently.
		virtual void draw(DrawList& draw_list) const = 0;

		// The object's geometry in world space, before view-pixel quantization.
		//
		// Reports an extent for spatial queries; CollisionObject::shape()
		// supplies detailed collision geometry. This is not a visibility flag:
		// draw() decides whether to emit anything.
		virtual RectangleF bounds() const = 0;

		// Conservative drawn extent for culling at this view's world units per
		// pixel (nonnegative). Ordinary geometry needs no adjustment. Drawables
		// that snap in view pixels override this: quantization follows the
		// camera, so its world-space allowance depends on zoom. bounds() remains
		// the unsnapped geometry used by layout and collision.
		virtual RectangleF cull_bounds(float /*units_per_pixel*/) const
		{
			return this->bounds();
		}
	};
}
