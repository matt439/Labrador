#include "samples/components/pointer_capture.h"

using namespace labrador;

namespace components
{
	PointerSample PointerCapture::update(const Mouse& mouse, const Touch& touch)
	{
		// Fingers down now or last tick, and one tick past that, keep the
		// mouse out: the window for a compatibility click to arrive in.
		const bool fingers = touch.state().count > 0 ||
			touch.previous_state().count > 0;
		const bool mouse_allowed = !fingers && this->quiet_ticks_ == 0;
		if (fingers)
		{
			this->quiet_ticks_ = 1;
		}
		else if (this->quiet_ticks_ > 0)
		{
			--this->quiet_ticks_;
		}

		if (this->owner_ == Owner::touch)
		{
			const uint32_t id = this->touch_id_;
			if (const TouchPoint* point = touch.state().find(id))
			{
				return { PointerPhase::held, point->position };
			}

			this->owner_ = Owner::none;
			this->touch_id_ = 0;
			if (touch.released(id))
			{
				return { PointerPhase::released,
					touch.previous_state().find(id)->position };
			}
			return { PointerPhase::cancelled, Vector2I() };
		}

		if (this->owner_ == Owner::mouse)
		{
			if (mouse.held(MouseButton::left))
			{
				return { PointerPhase::held, mouse.position() };
			}

			this->owner_ = Owner::none;
			if (mouse.released(MouseButton::left))
			{
				return { PointerPhase::released, mouse.position() };
			}
			return { PointerPhase::cancelled, Vector2I() };
		}

		for (const TouchPoint& point : touch.points())
		{
			if (touch.pressed(point.id))
			{
				this->owner_ = Owner::touch;
				this->touch_id_ = point.id;
				return { PointerPhase::pressed, point.position };
			}
		}

		if (mouse_allowed && mouse.pressed(MouseButton::left))
		{
			this->owner_ = Owner::mouse;
			return { PointerPhase::pressed, mouse.position() };
		}

		return {};
	}

	void PointerCapture::cancel()
	{
		this->owner_ = Owner::none;
		this->touch_id_ = 0;
	}

	bool PointerCapture::active() const
	{
		return this->owner_ != Owner::none;
	}

	std::optional<size_t> hit_target(std::span<const PointerTarget> targets,
		const Vector2F& point)
	{
		for (size_t index = 0; index < targets.size(); ++index)
		{
			const PointerTarget& target = targets[index];
			const RectangleF& box = target.bounds;
			if (target.enabled && point.x >= box.x && point.y >= box.y &&
				point.x < box.x + box.width && point.y < box.y + box.height)
			{
				return index;
			}
		}
		return std::nullopt;
	}
}
