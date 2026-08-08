#include "engine/render/animation_object.h"

#include <stdexcept>
#include <string>

using namespace mattmath;

namespace artattack
{
	AnimationObject::AnimationObject(const std::string& sheet_name,
		const std::string& animation_strip_name,
		RenderResources* render_resources,
		const Colour& color,
		float rotation,
		const Vector2F& origin,
		SpriteFlip flip,
		float layer_depth) :
		SpriteSheetObject(sheet_name, render_resources, color, rotation,
			origin, flip, layer_depth),
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

	void AnimationObject::draw(DrawList& draw_list,
		const RectangleF& destination_rectangle) const
	{
		SpriteSheetObject::sprite_sheet()->draw(draw_list,
			this->source_rectangle(),
			destination_rectangle,
			this->colour(),
			this->draw_rotation(),
			this->origin(),
			this->flip(),
			this->layer_depth());
	}
	void AnimationObject::draw(DrawList& draw_list,
		const Vector2F& position, float scale) const
	{
		SpriteSheetObject::sprite_sheet()->draw(draw_list,
			this->source_rectangle(),
			position,
			this->colour(),
			this->draw_rotation(),
			this->origin(),
			scale,
			this->flip(),
			this->layer_depth());
	}

	void AnimationObject::draw_with(DrawList& draw_list,
		const RectangleF& destination_rectangle,
		const Colour& colour,
		SpriteFlip flip) const
	{
		SpriteSheetObject::sprite_sheet()->draw(draw_list,
			this->source_rectangle(),
			destination_rectangle,
			colour,
			this->draw_rotation(),
			this->origin(),
			flip,
			this->layer_depth());
	}

	void AnimationObject::update(float dt)
	{
		if (this->paused_)
		{
			return;
		}
		const AnimationStrip& animation_strip = this->animation_strip();
		this->time_elapsed_ += dt;
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
			throw std::out_of_range("Animation frame index " +
				std::to_string(frame_index) + " is outside a strip of " +
				std::to_string(this->animation_strip().frame_count()) + ".");
		}
		this->frame_index_ = frame_index;
	}
	bool AnimationObject::is_paused() const
	{
		return this->paused_;
	}
	const RectangleI& AnimationObject::source_rectangle() const
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