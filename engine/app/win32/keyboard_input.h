#pragma once

#include "engine/input/keyboard.h"

#include <cstdint>
#include <optional>

namespace labrador
{
	// Physical key position from a Win32 key message. Pause/NumLock use the
	// virtual key; other positions use the scan code and extended bit.
	// Unknown positions return Key::none. No window or device is accessed.
	Key key_from_message(uint64_t w_param, intptr_t l_param);

	// Per-window assembly of WM_CHAR's UTF-16 units into the keyboard's
	// UTF-32 input. A high surrogate emits nothing and replaces any pending
	// high. A following low combines with it; any other non-high discards it.
	// An unpaired low is forwarded for Keyboard::on_text to replace.
	class Utf16Input
	{
	public:
		// No value means a stored high surrogate; U+0000 is an emitted value.
		// No implicit flush or focus reset: the owning window keeps this state.
		std::optional<char32_t> append(char16_t unit);

	private:
		char16_t pending_high_surrogate_ = 0;
	};
}
