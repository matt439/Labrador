#include "engine/render/animation_object.h"

using namespace DirectX;
using namespace mattmath;

AnimationObject::AnimationObject(const float* dt,
	const std::string& sheet_name,
	const std::string& animation_strip_name,
	RenderResources* render_resources,
	const Colour& color,
	float rotation,
	const Vector2F& origin,
	SpriteEffects effects,
	float layer_depth) :
	SpriteSheetObject(sheet_name, render_resources, color, rotation,
		origin, effects, layer_depth),
	dt_(dt),
	// The base is complete by now, so its sheet is there to resolve against.
	strip_(SpriteSheetObject::get_sprite_sheet()->
		resolve_animation_strip(animation_strip_name))
{
	this->frame_time_ = this->get_animation_strip().get_frame_time();
}

const AnimationStrip& AnimationObject::get_animation_strip() const
{
	return this->get_sprite_sheet()->get_animation_strip(this->strip_);
}

void AnimationObject::draw(SpriteBatch* sprite_batch,
	const RectangleI& destination_rectangle) const
{
	SpriteSheet* sprite_sheet = SpriteSheetObject::get_sprite_sheet();

	sprite_sheet->draw(sprite_batch,
		this->get_source_rectangle(),
		destination_rectangle,
		this->get_colour(),
		this->get_draw_rotation(),
		this->get_origin(),
		this->get_effects(),
		this->get_layer_depth());
}
void AnimationObject::draw(SpriteBatch* sprite_batch,
	const RectangleF& destination_rectangle) const
{
	this->draw(sprite_batch, destination_rectangle.get_rectangle_i());
}
void AnimationObject::draw(SpriteBatch* sprite_batch,
	const Vector2F& position, float scale) const
{
	SpriteSheet* sprite_sheet = SpriteSheetObject::get_sprite_sheet();

	sprite_sheet->draw(sprite_batch,
		this->get_source_rectangle(),
		position,
		this->get_colour(),
		this->get_draw_rotation(),
		this->get_origin(),
		scale,
		this->get_effects(),
		this->get_layer_depth());

}
void AnimationObject::draw(SpriteBatch* sprite_batch,
	const RectangleF& destination_rectangle,
	const Camera& camera) const
{
	RectangleF rect = camera.calculate_view_rectangle(destination_rectangle);
	this->draw(sprite_batch, rect);
}
void AnimationObject::draw(SpriteBatch* sprite_batch,
	const Vector2F& position,
	const Camera& camera, float scale) const
{
	Vector2F view_pos = camera.calculate_view_position(position);
	float view_scale = camera.calculate_view_scale(scale);
	this->draw(sprite_batch, view_pos, view_scale);
}

void AnimationObject::draw_with(SpriteBatch* sprite_batch,
	const RectangleF& destination_rectangle,
	const Camera& camera,
	const Colour& colour,
	SpriteEffects effects) const
{
	SpriteSheet* sprite_sheet = SpriteSheetObject::get_sprite_sheet();

	sprite_sheet->draw(sprite_batch,
		this->get_source_rectangle(),
		camera.calculate_view_rectangle(destination_rectangle).get_rectangle_i(),
		colour,
		this->get_draw_rotation(),
		this->get_origin(),
		effects,
		this->get_layer_depth());
}

void AnimationObject::update()
{
	if (this->paused_)
	{
		return;
	}
	const AnimationStrip& animation_strip = this->get_animation_strip();
	this->time_elapsed_ += *this->dt_;
	float frame_time = this->frame_time_;
	if (this->time_elapsed_ > frame_time)
	{
		this->frame_index_++;
		if (this->frame_index_ >= animation_strip.get_frame_count())
		{
			if (animation_strip.get_looping())
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
void AnimationObject::reset()
{
	this->frame_index_ = 0;
	this->time_elapsed_ = 0.0f;
}
void AnimationObject::stop()
{
	this->paused_ = true;
	this->reset();
}
void AnimationObject::play()
{
	this->paused_ = false;
}
void AnimationObject::pause()
{
	this->paused_ = true;
}
void AnimationObject::set_frame_index(int frame_index)
{
	if (frame_index < 0 ||
		frame_index >= this->get_animation_strip().get_frame_count())
	{
		throw std::exception("Invalid frame index.");
	}
	this->frame_index_ = frame_index;
}
bool AnimationObject::is_paused() const
{
	return this->paused_;
}
const RECT* AnimationObject::get_source_rectangle() const
{
	return this->get_animation_strip().get_frame_rect(this->frame_index_);

}
void AnimationObject::set_animation_strip_and_reset(const std::string& sprite_sheet,
	const std::string& animation_strip)
{
	this->set_sprite_sheet(sprite_sheet);
	this->strip_ = this->get_sprite_sheet()->resolve_animation_strip(
		animation_strip);
	this->set_frame_time_to_default();
	this->reset();
}
void AnimationObject::set_frame_time(float frame_time)
{
	this->frame_time_ = frame_time;
}
void AnimationObject::set_frame_time_to_default()
{
	this->frame_time_ = this->get_animation_strip().get_frame_time();
}