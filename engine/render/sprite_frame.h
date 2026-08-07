#pragma once

#include "engine/math/matt_math.h"

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
	const RECT* source_rectangle() const;
private:
	RECT source_rectangle_ = { 0, 0, 0, 0 };

	mattmath::RectangleI source_rectangle2_ = mattmath::RectangleI::ZERO;
	mattmath::Vector2F origin_ = mattmath::Vector2F::ZERO;
	bool rotated_ = false;

	void set_source_rectangle(
		const mattmath::RectangleI& source_rectangle);
	void set_source_rectangle(const mattmath::Vector2F& position,
		const mattmath::Vector2F& size);
};
