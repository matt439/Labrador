#include "engine/app/web/dom_events.h"

#include <cstddef>

namespace labrador
{
	namespace
	{
		struct CodeKey
		{
			std::string_view code;
			Key key;
		};

		// The same positions the Win32 window reads from set-1 scan codes,
		// spelt as the UI Events KeyboardEvent code values name them. Both
		// halves of a pair the engine does not tell apart - the two Shifts,
		// the two Controls, the two Alts, and the two Enters - answer one Key,
		// as they do on Windows.
		constexpr CodeKey code_keys[] = {
			{ "KeyA", Key::a }, { "KeyB", Key::b }, { "KeyC", Key::c },
			{ "KeyD", Key::d }, { "KeyE", Key::e }, { "KeyF", Key::f },
			{ "KeyG", Key::g }, { "KeyH", Key::h }, { "KeyI", Key::i },
			{ "KeyJ", Key::j }, { "KeyK", Key::k }, { "KeyL", Key::l },
			{ "KeyM", Key::m }, { "KeyN", Key::n }, { "KeyO", Key::o },
			{ "KeyP", Key::p }, { "KeyQ", Key::q }, { "KeyR", Key::r },
			{ "KeyS", Key::s }, { "KeyT", Key::t }, { "KeyU", Key::u },
			{ "KeyV", Key::v }, { "KeyW", Key::w }, { "KeyX", Key::x },
			{ "KeyY", Key::y }, { "KeyZ", Key::z },

			{ "Digit0", Key::digit_0 }, { "Digit1", Key::digit_1 },
			{ "Digit2", Key::digit_2 }, { "Digit3", Key::digit_3 },
			{ "Digit4", Key::digit_4 }, { "Digit5", Key::digit_5 },
			{ "Digit6", Key::digit_6 }, { "Digit7", Key::digit_7 },
			{ "Digit8", Key::digit_8 }, { "Digit9", Key::digit_9 },

			{ "F1", Key::f1 }, { "F2", Key::f2 }, { "F3", Key::f3 },
			{ "F4", Key::f4 }, { "F5", Key::f5 }, { "F6", Key::f6 },
			{ "F7", Key::f7 }, { "F8", Key::f8 }, { "F9", Key::f9 },
			{ "F10", Key::f10 }, { "F11", Key::f11 }, { "F12", Key::f12 },

			{ "Escape", Key::escape },
			{ "Tab", Key::tab },
			{ "CapsLock", Key::caps_lock },
			{ "ShiftLeft", Key::shift }, { "ShiftRight", Key::shift },
			{ "ControlLeft", Key::control }, { "ControlRight", Key::control },
			{ "AltLeft", Key::alt }, { "AltRight", Key::alt },
			{ "Space", Key::space },
			{ "Enter", Key::enter }, { "NumpadEnter", Key::enter },
			{ "Backspace", Key::backspace },

			{ "Insert", Key::insert },
			{ "Delete", Key::del },
			{ "Home", Key::home },
			{ "End", Key::end },
			{ "PageUp", Key::page_up },
			{ "PageDown", Key::page_down },

			{ "ArrowLeft", Key::left },
			{ "ArrowRight", Key::right },
			{ "ArrowUp", Key::up },
			{ "ArrowDown", Key::down },

			{ "PrintScreen", Key::print_screen },
			{ "ScrollLock", Key::scroll_lock },
			{ "Pause", Key::pause },
			{ "NumLock", Key::num_lock },

			{ "Minus", Key::minus },
			{ "Equal", Key::equals },
			{ "BracketLeft", Key::left_bracket },
			{ "BracketRight", Key::right_bracket },
			{ "Backslash", Key::backslash },
			{ "Semicolon", Key::semicolon },
			{ "Quote", Key::apostrophe },
			{ "Backquote", Key::grave },
			{ "Comma", Key::comma },
			{ "Period", Key::period },
			{ "Slash", Key::slash },

			{ "Numpad0", Key::numpad_0 }, { "Numpad1", Key::numpad_1 },
			{ "Numpad2", Key::numpad_2 }, { "Numpad3", Key::numpad_3 },
			{ "Numpad4", Key::numpad_4 }, { "Numpad5", Key::numpad_5 },
			{ "Numpad6", Key::numpad_6 }, { "Numpad7", Key::numpad_7 },
			{ "Numpad8", Key::numpad_8 }, { "Numpad9", Key::numpad_9 },
			{ "NumpadAdd", Key::numpad_add },
			{ "NumpadSubtract", Key::numpad_subtract },
			{ "NumpadMultiply", Key::numpad_multiply },
			{ "NumpadDivide", Key::numpad_divide },
			{ "NumpadDecimal", Key::numpad_decimal },
		};

		// The one code point `text` holds, or zero if it holds anything else:
		// nothing, several characters, or bytes that are not UTF-8.
		char32_t single_code_point(std::string_view text)
		{
			if (text.empty())
			{
				return 0;
			}

			const unsigned char lead = static_cast<unsigned char>(text[0]);
			std::size_t length = 0;
			char32_t value = 0;
			char32_t minimum = 0;
			if (lead < 0x80u)
			{
				length = 1;
				value = lead;
			}
			else if ((lead & 0xE0u) == 0xC0u)
			{
				length = 2;
				value = lead & 0x1Fu;
				minimum = 0x80u;
			}
			else if ((lead & 0xF0u) == 0xE0u)
			{
				length = 3;
				value = lead & 0x0Fu;
				minimum = 0x800u;
			}
			else if ((lead & 0xF8u) == 0xF0u)
			{
				length = 4;
				value = lead & 0x07u;
				minimum = 0x10000u;
			}
			else
			{
				return 0;
			}

			if (text.size() != length)
			{
				return 0;
			}

			for (std::size_t index = 1; index < length; ++index)
			{
				const unsigned char unit = static_cast<unsigned char>(text[index]);
				if ((unit & 0xC0u) != 0x80u)
				{
					return 0;
				}
				value = (value << 6) | (unit & 0x3Fu);
			}

			// Overlong forms, surrogates and anything past the last plane are
			// not characters, however they were encoded.
			if (value < minimum || value > 0x10FFFFu ||
				(value >= 0xD800u && value <= 0xDFFFu))
			{
				return 0;
			}
			return value;
		}
	}

	Key key_from_code(std::string_view code)
	{
		for (const CodeKey& entry : code_keys)
		{
			if (entry.code == code)
			{
				return entry.key;
			}
		}
		return Key::none;
	}

	char32_t text_from_key(std::string_view key, bool control, bool alt,
		bool meta)
	{
		if (meta || control != alt)
		{
			return 0;
		}
		return single_code_point(key);
	}

	bool browser_keeps_key(Key key, bool control, bool alt, bool meta)
	{
		if (control || alt || meta)
		{
			return true;
		}
		return key == Key::none || key == Key::tab ||
			(key >= Key::f1 && key <= Key::f12);
	}

	MouseButton mouse_button_from_dom(int button)
	{
		switch (button)
		{
		case 0: return MouseButton::left;
		case 1: return MouseButton::middle;
		case 2: return MouseButton::right;
		case 3: return MouseButton::x1;
		case 4: return MouseButton::x2;
		default: return MouseButton::none;
		}
	}

	float wheel_notches(double delta, unsigned int delta_mode)
	{
		// DOM_DELTA_PIXEL, DOM_DELTA_LINE and DOM_DELTA_PAGE, by value: this
		// file is compiled where the header that names them is, but it has no
		// other reason to include it.
		switch (delta_mode)
		{
		case 0: return static_cast<float>(delta / 100.0);
		case 1: return static_cast<float>(delta / 3.0);
		case 2: return static_cast<float>(delta);
		default: return 0.0f;
		}
	}
}
