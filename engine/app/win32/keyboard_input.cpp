#include "engine/app/win32/keyboard_input.h"

#include <Windows.h>

namespace labrador
{
	// Set-1 scan positions preserve physical bindings across keyboard layouts.
	// The extended bit distinguishes the navigation cluster from the numpad;
	// WM_CHAR remains the layout-aware text channel. Unknown positions drop.
	Key key_from_message(uint64_t w_param, intptr_t l_param)
	{
		// Pause and NumLock share scan 0x45; Windows identifies the E1 Pause
		// sequence through its virtual key rather than the E0 extended bit.
		if (w_param == VK_PAUSE) { return Key::pause; }
		if (w_param == VK_NUMLOCK) { return Key::num_lock; }
		const unsigned int scan = (static_cast<unsigned long long>(l_param) >> 16) & 0xffu;
		const bool extended = (l_param & (1LL << 24)) != 0;
		if (extended)
		{
			switch (scan)
			{
			case 0x1c: return Key::enter;
			case 0x1d: return Key::control;
			case 0x35: return Key::numpad_divide;
			case 0x37: return Key::print_screen;
			case 0x38: return Key::alt;
			case 0x47: return Key::home;
			case 0x48: return Key::up;
			case 0x49: return Key::page_up;
			case 0x4b: return Key::left;
			case 0x4d: return Key::right;
			case 0x4f: return Key::end;
			case 0x50: return Key::down;
			case 0x51: return Key::page_down;
			case 0x52: return Key::insert;
			case 0x53: return Key::del;
			default: return Key::none;
			}
		}
		switch (scan)
		{
		case 0x01: return Key::escape;
		case 0x02: return Key::digit_1;
		case 0x03: return Key::digit_2;
		case 0x04: return Key::digit_3;
		case 0x05: return Key::digit_4;
		case 0x06: return Key::digit_5;
		case 0x07: return Key::digit_6;
		case 0x08: return Key::digit_7;
		case 0x09: return Key::digit_8;
		case 0x0a: return Key::digit_9;
		case 0x0b: return Key::digit_0;
		case 0x0c: return Key::minus;
		case 0x0d: return Key::equals;
		case 0x0e: return Key::backspace;
		case 0x0f: return Key::tab;
		case 0x10: return Key::q;
		case 0x11: return Key::w;
		case 0x12: return Key::e;
		case 0x13: return Key::r;
		case 0x14: return Key::t;
		case 0x15: return Key::y;
		case 0x16: return Key::u;
		case 0x17: return Key::i;
		case 0x18: return Key::o;
		case 0x19: return Key::p;
		case 0x1a: return Key::left_bracket;
		case 0x1b: return Key::right_bracket;
		case 0x1c: return Key::enter;
		case 0x1d: return Key::control;
		case 0x1e: return Key::a;
		case 0x1f: return Key::s;
		case 0x20: return Key::d;
		case 0x21: return Key::f;
		case 0x22: return Key::g;
		case 0x23: return Key::h;
		case 0x24: return Key::j;
		case 0x25: return Key::k;
		case 0x26: return Key::l;
		case 0x27: return Key::semicolon;
		case 0x28: return Key::apostrophe;
		case 0x29: return Key::grave;
		case 0x2a: return Key::shift;
		case 0x2b: return Key::backslash;
		case 0x2c: return Key::z;
		case 0x2d: return Key::x;
		case 0x2e: return Key::c;
		case 0x2f: return Key::v;
		case 0x30: return Key::b;
		case 0x31: return Key::n;
		case 0x32: return Key::m;
		case 0x33: return Key::comma;
		case 0x34: return Key::period;
		case 0x35: return Key::slash;
		case 0x36: return Key::shift;
		case 0x37: return Key::numpad_multiply;
		case 0x38: return Key::alt;
		case 0x39: return Key::space;
		case 0x3a: return Key::caps_lock;
		case 0x3b: return Key::f1;
		case 0x3c: return Key::f2;
		case 0x3d: return Key::f3;
		case 0x3e: return Key::f4;
		case 0x3f: return Key::f5;
		case 0x40: return Key::f6;
		case 0x41: return Key::f7;
		case 0x42: return Key::f8;
		case 0x43: return Key::f9;
		case 0x44: return Key::f10;
		case 0x45: return Key::num_lock;
		case 0x46: return Key::scroll_lock;
		case 0x47: return Key::numpad_7;
		case 0x48: return Key::numpad_8;
		case 0x49: return Key::numpad_9;
		case 0x4a: return Key::numpad_subtract;
		case 0x4b: return Key::numpad_4;
		case 0x4c: return Key::numpad_5;
		case 0x4d: return Key::numpad_6;
		case 0x4e: return Key::numpad_add;
		case 0x4f: return Key::numpad_1;
		case 0x50: return Key::numpad_2;
		case 0x51: return Key::numpad_3;
		case 0x52: return Key::numpad_0;
		case 0x53: return Key::numpad_decimal;
		case 0x57: return Key::f11;
		case 0x58: return Key::f12;
		default: return Key::none;
		}
	}

	std::optional<char32_t> Utf16Input::append(char16_t unit)
	{
		if (unit >= 0xD800 && unit <= 0xDBFF)
		{
			this->pending_high_surrogate_ = unit;
			return std::nullopt;
		}

		char32_t codepoint = static_cast<char32_t>(unit);
		if (unit >= 0xDC00 && unit <= 0xDFFF &&
			this->pending_high_surrogate_ != 0)
		{
			codepoint = 0x10000u +
				((static_cast<char32_t>(
					this->pending_high_surrogate_) - 0xD800u) << 10) +
				(static_cast<char32_t>(unit) - 0xDC00u);
		}

		this->pending_high_surrogate_ = 0;
		return codepoint;
	}
}
