#ifndef TEXTUREOBJECT_H
#define TEXTUREOBJECT_H

#include "engine/render/sprite_sheet_object.h"

class TextureObject : public SpriteSheetObject
{
public:
	TextureObject() = default;
	TextureObject(const std::string& sheet_name,
		const std::string& frame_name,
		ResourceManager* resource_manager,
		const MattMath::Colour& color = colour_consts::WHITE,
		float rotation = 0.0f,
		const MattMath::Vector2F& origin = MattMath::Vector2F::ZERO,
		DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
		float layer_depth = 0.0f);

protected:
	virtual void draw(DirectX::SpriteBatch* sprite_batch,
		const MattMath::RectangleI& destination_rectangle) const;
	virtual void draw(DirectX::SpriteBatch* sprite_batch,
		const MattMath::RectangleF& destination_rectangle) const;
	virtual void draw(DirectX::SpriteBatch* sprite_batch,
		const MattMath::Vector2F& position, float scale = 1.0f) const;

	virtual void draw(DirectX::SpriteBatch* sprite_batch,
		const MattMath::RectangleF& destination_rectangle,
		const MattMath::Camera& camera) const;
	virtual void draw(DirectX::SpriteBatch* sprite_batch,
		const MattMath::Vector2F& position,
		const MattMath::Camera& camera, float scale = 1.0f) const;

	// Draws without reading any of the per-draw members, so callers can compute
	// element name / colour / origin / effects / rotation into locals instead
	// of assigning them to this object first.
	//
	// Level::draw_active_level runs draw() on the SAME object from every render
	// worker simultaneously, so "set members, then draw" is an unsynchronised
	// data race - and on the std::string element name it is concurrent
	// free/allocate on one control block, i.e. heap corruption.
	void draw_with(DirectX::SpriteBatch* sprite_batch,
		const MattMath::RectangleF& destination_rectangle,
		const MattMath::Camera& camera,
		const std::string& element_name,
		const MattMath::Colour& colour,
		const MattMath::Vector2F& origin,
		DirectX::SpriteEffects effects,
		float rotation) const;
};
#endif // !TEXTUREOBJECT_H
