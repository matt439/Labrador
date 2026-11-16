#include "samples/components/gallery_state.h"

#include "engine/input/gamepads.h"
#include "engine/input/keyboard.h"
#include "engine/render/text_encoding.h"

#include <string>
#include <tuple>

using namespace labrador;

namespace components
{
	void GalleryState::begin_name_edit()
	{
		if (this->name_->begin_edit())
		{
			this->set_name_status(L"Name editing: " + widen(this->name_->draft()));
		}
	}

	void GalleryState::cancel_name_edit()
	{
		if (this->name_ != nullptr && this->name_->cancel())
		{
			this->set_name_status(L"Name kept: " + widen(this->name_->value()));
			this->repeat_.start_held(this->held_direction());
		}
	}

	void GalleryState::set_name_status(const std::wstring& text)
	{
		if (this->name_status_->text() == text)
		{
			return;
		}
		this->name_status_->set_text(text);
		if (this->on_status_)
		{
			this->on_status_(text);
		}
	}

	void GalleryState::update_name_edit()
	{
		const Keyboard& keyboard = *this->app_->keyboard();
		const Gamepads& pads = *this->app_->gamepads();
		if (!keyboard.focused() || this->focus_.focused(0) != this->name_ ||
			keyboard.pressed(Key::escape) || pads.pressed(0, GamepadButton::b))
		{
			this->cancel_name_edit();
			return;
		}
		// Commands and typed text are separate snapshots. Cancellation wins;
		// Backspace wins the remaining bucket, without claiming event ordering.
		if (keyboard.pressed(Key::backspace))
		{
			if (this->name_->backspace())
			{
				this->set_name_status(L"Name editing: " + widen(this->name_->draft()));
			}
			return;
		}
		std::string typed;
		for (char byte : keyboard.typed())
		{
			const unsigned char value = static_cast<unsigned char>(byte);
			// Win32 can deliver WM_CHAR for Enter and other command keys.
			// UTF-8 continuation bytes remain intact for the field to validate.
			if (value >= 0x20 && value != 0x7F)
			{
				typed.push_back(byte);
			}
		}
		const TextEdit result = this->name_->append(typed);
		switch (result)
		{
		case TextEdit::accepted:
			this->set_name_status(L"Name editing: " + widen(this->name_->draft()));
			break;
		case TextEdit::unchanged:
			break;
		case TextEdit::invalid_utf8:
			this->set_name_status(L"Name: invalid UTF-8");
			return;
		case TextEdit::unsupported_character:
			this->set_name_status(L"Name: single-line BMP only");
			return;
		case TextEdit::missing_glyph:
			this->set_name_status(L"Name: missing font glyph");
			return;
		case TextEdit::length_limit:
			this->set_name_status(L"Name: 12 chars maximum");
			return;
		case TextEdit::width_limit:
			this->set_name_status(L"Name: field is full");
			return;
		case TextEdit::inactive:
			return;
		}
		if (keyboard.pressed(Key::enter) || pads.pressed(0, GamepadButton::a))
		{
			std::ignore = this->name_->confirm();
			this->set_name_status(L"Name saved: " + widen(this->name_->value()));
			this->repeat_.start_held(this->held_direction());
		}
	}

}
