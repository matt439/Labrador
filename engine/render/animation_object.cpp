#include "engine/render/animation_object.h"

using namespace DirectX;
using namespace mattmath;

namespace artattack
{
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
		strip_(SpriteSheetObject::sprite_sheet()->
			resolve_animation_strip(animation_strip_name))
	{
		this->frame_time_ = this->animation_strip().frame_time();
	}

	const AnimationStrip& AnimationObject::animation_strip() const
	{
		return this->sprite_sheet()->animation_strip(this->strip_);
	}

	void AnimationObject::draw(SpriteBatch* sprite_batch,
		const RectangleI& destination_rectangle) const
	{
		SpriteSheet* sprite_sheet = SpriteSheetObject::sprite_sheet();

		sprite_sheet->draw(sprite_batch,
			this->source_rectangle(),
			destination_rectangle,
			this->colour(),
			this->draw_rotation(),
			this->origin(),
			this->effects(),
			this->layer_depth());
	}
	void AnimationObject::draw(SpriteBatch* sprite_batch,
		const RectangleF& destination_rectangle) const
	{
		this->draw(sprite_batch, destination_rectangle.rectangle_i());
	}
	void AnimationObject::draw(SpriteBatch* sprite_batch,
		const Vector2F& position, float scale) const
	{
		SpriteSheet* sprite_sheet = SpriteSheetObject::sprite_sheet();

		sprite_sheet->draw(sprite_batch,
			this->source_rectangle(),
			position,
			this->colour(),
			this->draw_rotation(),
			this->origin(),
			scale,
			this->effects(),
			this->layer_depth());

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
		SpriteSheet* sprite_sheet = SpriteSheetObject::sprite_sheet();

		sprite_sheet->draw(sprite_batch,
			this->source_rectangle(),
			camera.calculate_view_rectangle(destination_rectangle).rectangle_i(),
			colour,
			this->draw_rotation(),
			this->origin(),
			effects,
			this->layer_depth());
	}

	void AnimationObject::update()
	{
		if (this->paused_)
		{
			return;
		}
		const AnimationStrip& animation_strip = this->animation_strip();
		this->time_elapsed_ += *this->dt_;
		float frame_time = this->frame_time_;
		if (this->time_elapsed_ > frame_time)
		{
			this->frame_index_++;
			if (this->frame_index_ >= animation_strip.frame_count())
			{
				if (animation_strip.looping())
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
			frame_index >= this->animation_strip().frame_count())
		{
			throw std::exception("Invalid frame index.");
		}
		this->frame_index_ = frame_index;
	}
	bool AnimationObject::is_paused() const
	{
		return this->paused_;
	}
	const RECT* AnimationObject::source_rectangle() const
	{
		return this->animation_strip().frame_rect(this->frame_index_);

	}
	void AnimationObject::set_animation_strip_and_reset(const std::string& sprite_sheet,
		const std::string& animation_strip)
	{
		this->set_sprite_sheet(sprite_sheet);
		this->strip_ = this->sprite_sheet()->resolve_animation_strip(
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
		this->frame_time_ = this->animation_strip().frame_time();
	}
}