#include "engine/render/animation_object.h"

using namespace DirectX;
using namespace MattMath;

AnimationObject::AnimationObject(const float* dt,
	const std::string& sheet_name,
	const std::string& animation_strip_name,
	RenderResources* render_resources,
	const Colour& color,
	float rotation,
	const Vector2F& origin,
	SpriteEffects effects,
	float layer_depth) :
	SpriteSheetObject(sheet_name, animation_strip_name,
		render_resources, color, rotation, origin, effects,
		layer_depth),
	_dt(dt)
{
	this->_frame_time = this->get_animation_strip()->get_frame_time();
}

const AnimationStrip* AnimationObject::get_animation_strip() const
{
	return this->get_sprite_sheet()->get_animation_strip(
		this->get_element_name());
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
	if (this->_paused)
	{
		return;
	}
	const AnimationStrip* animation_strip = this->get_animation_strip();
	this->_time_elapsed += *this->_dt;
	float frame_time = this->_frame_time;
	if (this->_time_elapsed > frame_time)
	{
		this->_frame_index++;
		if (this->_frame_index >= animation_strip->get_frame_count())
		{
			if (animation_strip->get_looping())
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
void AnimationObject::reset()
{
	this->_frame_index = 0;
	this->_time_elapsed = 0.0f;
}
void AnimationObject::stop()
{
	this->_paused = true;
	this->reset();
}
void AnimationObject::play()
{
	this->_paused = false;
}
void AnimationObject::pause()
{
	this->_paused = true;
}
void AnimationObject::set_frame_index(int frame_index)
{
	if (frame_index < 0 ||
		frame_index >= this->get_animation_strip()->get_frame_count())
	{
		throw std::exception("Invalid frame index.");
	}
	this->_frame_index = frame_index;
}
bool AnimationObject::is_paused() const
{
	return this->_paused;
}
const RECT* AnimationObject::get_source_rectangle() const
{
	return this->get_animation_strip()->get_frame_rect(this->_frame_index);

}
void AnimationObject::set_animation_strip_and_reset(const std::string& sprite_sheet,
	const std::string& animation_strip)
{
	this->set_sprite_sheet_name(sprite_sheet);
	this->set_element_name(animation_strip);
	this->set_frame_time_to_default();
	this->reset();
}
void AnimationObject::set_frame_time(float frame_time)
{
	this->_frame_time = frame_time;
}
void AnimationObject::set_frame_time_to_default()
{
	this->_frame_time = this->get_animation_strip()->get_frame_time();
}