#pragma once

#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"

namespace artattack
{
	class SpriteFrame
	{
	public:
		SpriteFrame() = default;
		explicit SpriteFrame(const mattmath::RectangleI& source_rectangle,
		                     const mattmath::Vector2F& origin =
			                     mattmath::Vector2F::ZERO,
		                     bool rotated = false);
		SpriteFrame(const mattmath::Vector2F& position,
			const mattmath::Vector2F& size,
			const mattmath::Vector2F& origin =
				mattmath::Vector2F::ZERO,
			bool rotated = false);

		// A RectangleI rather than the RECT* this used to hand out. The RECT
		// was here only because SpriteBatch::Draw wanted one; the frame kept
		// both forms of the same rectangle so it had a stable address to
		// return, and the two could drift. The seam takes a RectangleI, so the
		// duplicate is gone and the conversion happens once, in the backend.
		const mattmath::RectangleI& source_rectangle() const;
	private:
		mattmath::RectangleI source_rectangle_ = mattmath::RectangleI::ZERO;
		mattmath::Vector2F origin_ = mattmath::Vector2F::ZERO;
		bool rotated_ = false;
	};
}
