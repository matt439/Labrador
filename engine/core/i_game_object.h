#pragma once

#include "engine/math/matt_math.h"
#include <SpriteBatch.h>

class IGameObject
{
public:
	virtual ~IGameObject() = default;
	virtual void update() = 0;
	virtual void draw(DirectX::SpriteBatch* sprite_batch, const MattMath::Camera& camera) = 0;
	virtual void draw(DirectX::SpriteBatch* sprite_batch) = 0;
	virtual bool is_visible_in_viewport(const MattMath::RectangleF& view) const = 0;
};
