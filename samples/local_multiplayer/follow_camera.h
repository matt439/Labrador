#pragma once

#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/camera.h"

namespace multiplayer
{
	// Game-owned following at scale 1. Copy this pair of files into a game;
	// the engine Camera supplies the transform, not the tracking policy.
	class FollowCamera
	{
	public:
		FollowCamera() = default;

		// The target is a world point. The inclusive dead zone is in local
		// viewport pixels, without the viewport's back-buffer offset. The first
		// update starts centred, then applies the zone and world constraints.
		// Later updates move only enough to bring the target into the zone.
		// Resizing preserves the old world-space view centre before following.
		//
		// World bounds win over the dead zone near an edge. A world axis no
		// larger than the viewport is centred, leaving equal space outside it.
		// All data and rectangle endpoints must be finite; viewport dimensions
		// must be positive, world/zone extents nonnegative and the zone inside
		// the viewport. Invalid data or an unrepresentable resulting translation
		// throws invalid_argument before any tracking state changes.
		void update(const labrador::Vector2F& target,
			const labrador::Vector2F& viewport_size,
			const labrador::RectangleF& dead_zone,
			const labrador::RectangleF& world);

		// Default camera before the first update. Read concurrently after
		// update has finished; this query and drawing change no tracking state.
		labrador::Camera camera() const;

	private:
		labrador::Vector2F translation_ = labrador::Vector2F::zero;
		labrador::Vector2F viewport_size_ = labrador::Vector2F::zero;
		bool initialized_ = false;
	};
}
