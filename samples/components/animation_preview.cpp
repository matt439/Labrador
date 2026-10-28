#include "samples/components/animation_preview.h"

#include "engine/render/animation_strip.h"
#include "engine/render/sprite_sheet.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace labrador;

namespace components
{
	AnimationPreview::AnimationPreview(const RenderResources& resources,
		RenderResources::SheetStrip strip) : resources_(&resources), strip_(strip)
	{
		const AnimationStrip& definition = resources.sprite_sheet(strip.sheet())
			->animation_strip(strip.strip());
		if (definition.frame_count() <= 0 ||
			!std::isfinite(definition.frame_time()) || definition.frame_time() <= 0.0f)
		{
			throw std::invalid_argument(
				"AnimationPreview needs a nonempty strip and a positive finite frame duration");
		}
		this->frame_count_ = definition.frame_count();
		this->frame_time_ = static_cast<double>(definition.frame_time());
		this->duration_ = this->frame_time_ * static_cast<double>(this->frame_count_);
		this->looping_ = definition.looping();
	}

	void AnimationPreview::update(float dt)
	{
		if (!std::isfinite(dt) || dt < 0.0f)
		{
			throw std::invalid_argument("AnimationPreview dt must be finite and nonnegative");
		}
		if (dt == 0.0f || this->finished_)
		{
			return;
		}

		// int count * finite float duration fits in double, as do two cycles.
		// Reduce dt first: adding a huge dt directly would erase a small phase.
		const double step = static_cast<double>(dt);
		const double next_elapsed = this->looping_
			? std::fmod(this->elapsed_ + std::fmod(step, this->duration_), this->duration_)
			: std::min(this->elapsed_ + step, this->duration_);
		const int last_frame = this->frame_count_ - 1;
		const double position = next_elapsed / this->frame_time_;
		// Clamp before conversion, including a rounded-up quotient at cycle end.
		const int next_frame = position >= static_cast<double>(last_frame)
			? last_frame : static_cast<int>(position);

		this->elapsed_ = next_elapsed;
		this->frame_index_ = next_frame;
		this->finished_ = !this->looping_ && next_elapsed >= this->duration_;
	}

	void AnimationPreview::restart()
	{
		this->elapsed_ = 0.0;
		this->frame_index_ = 0;
		this->finished_ = false;
	}

	double AnimationPreview::elapsed() const { return this->elapsed_; }
	int AnimationPreview::frame_index() const { return this->frame_index_; }
	bool AnimationPreview::finished() const { return this->finished_; }

	void AnimationPreview::draw(DrawList& draw_list,
		const RectangleF& destination, const Colour& colour) const
	{
		const SpriteSheet* sheet = this->resources_->sprite_sheet(this->strip_.sheet());
		const AnimationStrip& strip = sheet->animation_strip(this->strip_.strip());
		sheet->draw(draw_list, strip.frame_rect(this->frame_index_), destination, colour);
	}
}
