#ifndef TEXTUREOBJECT_H
#define TEXTUREOBJECT_H

#include "engine/render/sprite_sheet_object.h"

// One still frame out of a sprite sheet.
//
// The frame name is resolved against the sheet once, at construction, and what
// is kept is a handle: an index into that sheet's frame table. Drawing is then
// two indexed loads - the sheet out of its registry slot, the frame out of the
// sheet - with no string touched on the way.
class TextureObject : public SpriteSheetObject
{
public:
	TextureObject() = default;
	TextureObject(const std::string& sheet_name,
		const std::string& frame_name,
		RenderResources* render_resources,
		const MattMath::Colour& color = colour_consts::WHITE,
		float rotation = 0.0f,
		const MattMath::Vector2F& origin = MattMath::Vector2F::ZERO,
		DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
		float layer_depth = 0.0f);

protected:
	SpriteSheet::frame_handle get_frame() const;

	// Both setters resolve, and the two-argument one moves sheet and frame
	// together because a frame handle taken from one sheet indexes nothing
	// meaningful in another. There is deliberately no way to change the sheet
	// on its own.
	//
	// It is not atomic: a bad frame name throws with the sheet already moved.
	// That is tolerable only because an unresolvable name is fatal - the
	// worker's exception is rethrown on the joining thread (ThreadPool::wait)
	// and the process goes down, so nothing ever draws the mismatched pair.
	void set_frame(const std::string& sheet_name,
		const std::string& frame_name);
	void set_frame(const std::string& frame_name);

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
	// frame / colour / origin / effects / rotation into locals instead of
	// assigning them to this object first.
	//
	// Level::draw_active_level runs draw() on the SAME object from every render
	// worker simultaneously, so "set members, then draw" is an unsynchronised
	// data race - and back when the element was a std::string it was concurrent
	// free/allocate on one control block, i.e. heap corruption. The frame is a
	// handle now, so passing it costs a register rather than a string copy.
	void draw_with(DirectX::SpriteBatch* sprite_batch,
		const MattMath::RectangleF& destination_rectangle,
		const MattMath::Camera& camera,
		SpriteSheet::frame_handle frame,
		const MattMath::Colour& colour,
		const MattMath::Vector2F& origin,
		DirectX::SpriteEffects effects,
		float rotation) const;

private:
	SpriteSheet::frame_handle _frame;
};
#endif // !TEXTUREOBJECT_H
