#include "samples/components/gallery_state.h"

#include "engine/input/gamepads.h"
#include "engine/input/keyboard.h"

#include <string>
#include <string_view>

using namespace labrador;

namespace components
{
	namespace
	{
		std::wstring action_title(GalleryAction action)
		{
			return action == GalleryAction::restart_previews ? L"Restart" : L"Reset difficulty";
		}

		std::wstring action_token(GalleryAction action)
		{
			return action == GalleryAction::restart_previews ? L"restart_previews" : L"reset_difficulty";
		}

		std::wstring key_token(Key key)
		{
			const std::string_view token = key_name(key);
			return std::wstring(token.begin(), token.end());
		}
	}

	void GalleryState::refresh_binding_value()
	{
		this->binding_->set_value(action_title(this->binding_action_) + L": " +
			key_token(this->bindings_.key(this->binding_action_)));
		if (this->animation_help_ != nullptr)
		{
			this->animation_help_->set_text(L"Restart: " + key_token(this->bindings_.key(GalleryAction::restart_previews)) +
				L" / pad X; difficulty reset: " + key_token(this->bindings_.key(GalleryAction::reset_difficulty)));
		}
	}

	void GalleryState::set_binding_status(const std::wstring& text)
	{
		this->binding_status_->set_text(text);
		if (this->on_status_)
		{
			this->on_status_(text);
		}
	}

	void GalleryState::begin_binding_capture()
	{
		if (this->binding_->capturing())
		{
			return;
		}
		this->cancel_name_edit();
		this->binding_->begin_capture();
		this->pointer_.cancel();
		this->pressed_row_ = nullptr;
		this->dragging_ = nullptr;
		this->repeat_.start_held(Direction::none);
		this->set_binding_status(L"Binding capture: release held keys; Esc/B cancels.");
	}

	void GalleryState::cancel_binding_capture(const std::wstring& reason)
	{
		if (!this->binding_->capturing())
		{
			return;
		}
		this->binding_->cancel_capture();
		this->refresh_binding_value();
		this->repeat_.start_held(this->held_direction());
		this->set_binding_status(reason);
	}

	bool GalleryState::adjust_binding_selection(Direction direction)
	{
		if (this->binding_->capturing() ||
			(direction != Direction::left && direction != Direction::right))
		{
			return false;
		}
		this->binding_action_ = this->binding_action_ == GalleryAction::restart_previews
			? GalleryAction::reset_difficulty : GalleryAction::restart_previews;
		this->refresh_binding_value();
		this->set_binding_status(L"Binding action: " + action_token(this->binding_action_));
		return true;
	}

	bool GalleryState::update_binding_input()
	{
		const Keyboard& keyboard = *this->app_->keyboard();
		const Gamepads& pads = *this->app_->gamepads();
		if (!this->binding_->capturing())
		{
			if (this->focus_.focused(0) == this->binding_ && keyboard.pressed(Key::backspace))
			{
				this->bindings_.reset();
				this->refresh_binding_value();
				this->set_binding_status(L"Bindings reset: restart_previews=r, reset_difficulty=d.");
				return true;
			}
			return false;
		}

		if (keyboard.pressed(Key::escape) || pads.pressed(0, GamepadButton::b))
		{
			this->cancel_binding_capture(L"Binding capture cancelled.");
			return true;
		}
		// A can open this keyboard-only row too. Keep its initial neutral
		// keyboard snapshot unarmed until that opener has been released.
		if (keyboard.state().focused && this->binding_->waiting_for_release() &&
			pads.held(0, GamepadButton::a))
		{
			return true;
		}
		const CaptureStep step = this->binding_->update_capture(keyboard.state(), keyboard.previous_state());
		if (step.kind == CaptureKind::focus_lost)
		{
			this->repeat_.start_held(Direction::none);
			this->set_binding_status(L"Binding capture cancelled: focus lost.");
		}
		else if (step.kind == CaptureKind::multiple)
		{
			this->set_binding_status(L"Binding rejected: press one key, then release all keys.");
		}
		else if (step.kind == CaptureKind::key)
		{
			const BindingResult result = this->bindings_.try_bind(this->binding_action_, step.key);
			if (result == BindingResult::changed || result == BindingResult::unchanged)
			{
				this->refresh_binding_value();
				this->binding_->cancel_capture();
				this->repeat_.start_held(this->held_direction());
				this->set_binding_status(L"Binding saved: " + action_token(this->binding_action_) +
					L"=" + key_token(step.key));
			}
			else if (result == BindingResult::duplicate)
			{
				this->set_binding_status(L"Binding rejected: duplicate " + key_token(step.key) + L"; choose another key.");
			}
			else
			{
				this->set_binding_status(L"Binding rejected: protected key; use a letter or number.");
			}
		}
		return true;
	}
}
