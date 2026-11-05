#include "engine/core/utf8.h"

namespace labrador
{
	namespace
	{
		constexpr char32_t replacement_character = 0xFFFDu;

		char utf8_byte(char32_t bits)
		{
			return static_cast<char>(static_cast<unsigned char>(bits));
		}
	}

	void append_utf8(std::string& text, char32_t code_point)
	{
		char32_t value = code_point;

		// The surrogate range is not a character in its own right - it only
		// exists to encode astral planes in UTF-16 - so one arriving here
		// means the pairing upstream did not happen.
		if (value > 0x10FFFFu || (value >= 0xD800u && value <= 0xDFFFu))
		{
			value = replacement_character;
		}

		if (value < 0x80u)
		{
			text.push_back(utf8_byte(value));
		}
		else if (value < 0x800u)
		{
			text.push_back(utf8_byte(0xC0u | (value >> 6)));
			text.push_back(utf8_byte(0x80u | (value & 0x3Fu)));
		}
		else if (value < 0x10000u)
		{
			text.push_back(utf8_byte(0xE0u | (value >> 12)));
			text.push_back(utf8_byte(0x80u | ((value >> 6) & 0x3Fu)));
			text.push_back(utf8_byte(0x80u | (value & 0x3Fu)));
		}
		else
		{
			text.push_back(utf8_byte(0xF0u | (value >> 18)));
			text.push_back(utf8_byte(0x80u | ((value >> 12) & 0x3Fu)));
			text.push_back(utf8_byte(0x80u | ((value >> 6) & 0x3Fu)));
			text.push_back(utf8_byte(0x80u | (value & 0x3Fu)));
		}
	}
}
