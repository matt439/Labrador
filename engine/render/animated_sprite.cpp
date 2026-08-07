#include "engine/render/animated_sprite.h"

using namespace DirectX;
using namespace mattmath;

AnimatedSprite::AnimatedSprite(SpriteSheet* sprite_sheet,
	const std::string& animation_strip_name,
	const float* dt) :
	dt_(dt)
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
		this->animation_strip_->frame_rect(this->frame_index_);

	this->sprite_sheet_->draw(
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
		this->animation_strip_->frame_rect(this->frame_index_);

	this->sprite_sheet_->draw(
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
	if (this->paused_)
	{
		return;
	}
	this->time_elapsed_ += *this->dt_;
	float frame_time = this->animation_strip_->frame_time();
	if (this->time_elapsed_ > frame_time)
	{
		this->frame_index_++;
		if (this->frame_index_ >= this->animation_strip_->frame_count())
		{
			if (this->animation_strip_->looping())
			{
				this->frame_index_ = 0;
			}
			else
			{
				this->frame_index_--;
				this->paused_ = true;
			}
		}
		this->time_elapsed_ -= frame_time;
	}
}
void AnimatedSprite::reset()
{
	this->frame_index_ = 0;
	this->time_elapsed_ = 0.0f;
}
void AnimatedSprite::stop()
{
	this->paused_ = true;
	this->reset();
}
void AnimatedSprite::play()
{
	this->paused_ = false;
}
void AnimatedSprite::pause()
{
	this->paused_ = true;
}
void AnimatedSprite::set_animation_strip(SpriteSheet* sprite_sheet,
	const std::string& animation_strip_name)
{
	this->sprite_sheet_ = sprite_sheet;
	// Resolved here and kept as a pointer rather than a handle: this class
	// already holds the sheet, and a sheet's strip table is built once by the
	// loader and never grows, so the reference stays good for its lifetime.
	this->animation_strip_ = &sprite_sheet->animation_strip(
		sprite_sheet->resolve_animation_strip(animation_strip_name));
}
void AnimatedSprite::set_frame_index(int frame_index)
{
	if (frame_index < 0 ||
		frame_index >= this->animation_strip_->frame_count())
	{
		throw std::exception("Invalid frame index.");
	}
	this->frame_index_ = frame_index;
}
bool AnimatedSprite::is_paused() const
{
	return this->paused_;
}