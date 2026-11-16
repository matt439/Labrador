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
			if (!mouse.focused() || mouse.state().button_epoch != this->mouse_epoch_)
			{
				this->cancel();
				return { PointerPhase::cancelled, Vector2I() };
			}
			if (this->mouse_release_)
			{
				const Vector2I position = *this->mouse_release_;
				this->cancel();
				return { PointerPhase::released, position };
			}
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
			this->mouse_epoch_ = mouse.state().button_epoch;
			if (mouse.released(MouseButton::left) && !mouse.held(MouseButton::left))
			{
				this->mouse_release_ = mouse.position();
			}
			return { PointerPhase::pressed, mouse.position() };
		}

		return {};
	}

	void PointerCapture::cancel()
	{
		this->owner_ = Owner::none;
		this->touch_id_ = 0;
		this->mouse_release_.reset();
	}

	bool PointerCapture::active() const
	{
		return this->owner_ != Owner::none;
	}

}
