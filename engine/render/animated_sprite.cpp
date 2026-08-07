#include "engine/render/animated_sprite.h"

using namespace DirectX;
using namespace MattMath;

AnimatedSprite::AnimatedSprite(SpriteSheet* sprite_sheet,
	const std::string& animation_strip_name,
	const float* dt) :
	_dt(dt)
{
	this->set_animation_strip(sprite_sheet, animation_strip_name);
}

void AnimatedSprite::draw(SpriteBatch* sprite_batch,
	const RectangleI& destination_rectangle,
	const Colour& color,
	float rotation,
	const Vector2F& origin,
	SpriteEffects effects,
	float layer_depth) const
{
	const RECT* source_rect =
		this->_animation_strip->get_frame_rect(this->_frame_index);

	this->_sprite_sheet->draw(
		sprite_batch,
		source_rect,
		destination_rectangle,
		color,
		rotation,
		origin,
		effects,
		layer_depth);
}
void AnimatedSprite::draw(SpriteBatch* sprite_batch,
	const Vector2F& position,
	const Colour& color,
	float rotation,
	const Vector2F& origin,
	float scale,
	SpriteEffects effects,
	float layer_depth) const
{
	const RECT* source_rect =
		this->_animation_strip->get_frame_rect(this->_frame_index);

	this->_sprite_sheet->draw(
		sprite_batch,
		source_rect,
		position,
		color,
		rotation,
		origin,
		scale,
		effects,
		layer_depth);
}
void AnimatedSprite::update()
{
	if (this->_paused)
	{
		return;
	}
	this->_time_elapsed += *this->_dt;
	float frame_time = this->_animation_strip->get_frame_time();
	if (this->_time_elapsed > frame_time)
	{
		this->_frame_index++;
		if (this->_frame_index >= this->_animation_strip->get_frame_count())
		{
			if (this->_animation_strip->get_looping())
			{
				this->_frame_index = 0;
			}
			else
			{
				this->_frame_index--;
				this->_paused = true;
			}
		}
		this->_time_elapsed -= frame_time;
	}
}
void AnimatedSprite::reset()
{
	this->_frame_index = 0;
	this->_time_elapsed = 0.0f;
}
void AnimatedSprite::stop()
{
	this->_paused = true;
	this->reset();
}
void AnimatedSprite::play()
{
	this->_paused = false;
}
void AnimatedSprite::pause()
{
	this->_paused = true;
}
void AnimatedSprite::set_animation_strip(SpriteSheet* sprite_sheet,
	const std::string& animation_strip_name)
{
	this->_sprite_sheet = sprite_sheet;
	// Resolved here and kept as a pointer rather than a handle: this class
	// already holds the sheet, and a sheet's strip table is built once by the
	// loader and never grows, so the reference stays good for its lifetime.
	this->_animation_strip = &sprite_sheet->get_animation_strip(
		sprite_sheet->resolve_animation_strip(animation_strip_name));
}
void AnimatedSprite::set_frame_index(int frame_index)
{
	if (frame_index < 0 ||
		frame_index >= this->_animation_strip->get_frame_count())
	{
		throw std::exception("Invalid frame index.");
	}
	this->_frame_index = frame_index;
}
bool AnimatedSprite::is_paused() const
{
	return this->_paused;
}