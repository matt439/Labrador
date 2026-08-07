#ifndef ANIMATIONOBJECT_H
#define ANIMATIONOBJECT_H

#include "engine/render/sprite_sheet_object.h"
#include "engine/render/animated_sprite.h"

class AnimationObject : public SpriteSheetObject
{
public:
	AnimationObject() = default;
	AnimationObject(const float* dt,
		const std::string& sheet_name,
		const std::string& animation_strip_name,
		ResourceManager* resource_manager,
		const MattMath::Colour& color = colour_consts::WHITE,
		float rotation = 0.0f,
		const MattMath::Vector2F& origin = MattMath::Vector2F::ZERO,
		DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
		float layer_depth = 0.0f);
protected:
	virtual void update();
	void reset();
	void stop();
	void play();
	void pause();
	void set_frame_time(float frame_time);
	void set_frame_time_to_default();
	bool is_paused() const;

	void set_frame_index(int frame_index);
	void set_animation_strip_and_reset(const std::string& sprite_sheet,
		const std::string& animation_strip);

	virtual void draw(DirectX::SpriteBatch* sprite_batch,
		const MattMath::RectangleI& destination_rectangle) const;
	virtual void draw(DirectX::SpriteBatch* sprite_batch,
		const MattMath::RectangleF& destination_rectangle) const;
	virtual void draw(DirectX::SpriteBatch* sprite_batch, const MattMath::Vector2F& position,
		float scale = 1.0f) const;

	virtual void draw(DirectX::SpriteBatch* sprite_batch,
		const MattMath::RectangleF& destination_rectangle,
		const MattMath::Camera& camera) const;
	virtual void draw(DirectX::SpriteBatch* sprite_batch,
		const MattMath::Vector2F& position,
		const MattMath::Camera& camera, float scale = 1.0f) const;

	// See TextureObject::draw_with - lets a caller vary colour and effects per
	// draw without assigning them to this shared object first.
	void draw_with(DirectX::SpriteBatch* sprite_batch,
		const MattMath::RectangleF& destination_rectangle,
		const MattMath::Camera& camera,
		const MattMath::Colour& colour,
		DirectX::SpriteEffects effects) const;

private:
	const float* _dt = nullptr;
	int _frame_index = 0;
	bool _paused = false;
	float _time_elapsed = 0.0f;
	float _frame_time = 0.0f;
	const AnimationStrip* get_animation_strip() const;
	const RECT* get_source_rectangle() const;
};
#endif // !ANIMATIONOBJECT_H
