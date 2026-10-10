#include "engine/render/text_encoding.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace labrador
{
	namespace
	{
		constexpr char32_t replacement_character = 0xFFFD;

		// The length of the sequence a lead byte opens, or 0 for a byte that
		// cannot open one: a continuation byte, the overlong leads C0 and C1,
		// and F5 to FF, which would encode past U+10FFFF.
		std::size_t sequence_length(unsigned char lead)
		{
			if (lead < 0x80)
			{
				return 1;
			}
			if (lead >= 0xC2 && lead <= 0xDF)
			{
				return 2;
			}
			if (lead >= 0xE0 && lead <= 0xEF)
			{
				return 3;
			}
			if (lead >= 0xF0 && lead <= 0xF4)
			{
				return 4;
			}
			return 0;
		}

		// Whether `byte` may follow `lead` as the sequence's second byte. The
		// narrowed ranges after E0, ED, F0 and F4 are what rule out overlong
		// forms, the surrogates and anything past U+10FFFF; every other
		// second byte, and every later one, is a plain continuation.
		bool valid_second(unsigned char lead, unsigned char byte)
		{
			switch (lead)
			{
			case 0xE0: return byte >= 0xA0 && byte <= 0xBF;
			case 0xED: return byte >= 0x80 && byte <= 0x9F;
			case 0xF0: return byte >= 0x90 && byte <= 0xBF;
			case 0xF4: return byte >= 0x80 && byte <= 0x8F;
			default:   return byte >= 0x80 && byte <= 0xBF;
			}
		}

		// UTF-16 whatever the width of wchar_t, which is 16 bits under MSVC
		// and 32 under clang on every other platform. text_encoding.h says
		// the render module counts in UTF-16 units, so a character past the
		// basic plane is two of them on every platform rather than one on
		// some.
		void append_utf16(std::wstring& wide, char32_t code_point)
		{
			if (code_point < 0x10000)
			{
				wide += static_cast<wchar_t>(code_point);
				return;
			}
			const char32_t offset = code_point - 0x10000;
			wide += static_cast<wchar_t>(0xD800 + (offset >> 10));
			wide += static_cast<wchar_t>(0xDC00 + (offset & 0x3FF));
		}
	}

	// Each ill-formed run becomes one U+FFFD, and the runs are cut exactly
	// where MultiByteToWideChar(CP_UTF8) cuts them, so a Windows build reads
	// text as it did when it called that and every other build reads it the
	// same way. That is the Unicode Standard's maximal-subpart practice
	// (chapter 3, "U+FFFD Substitution of Maximal Subparts") with one
	// departure, Windows': a continuation byte that the narrowed range after
	// E0, ED, F0 or F4 refuses is swallowed with its lead rather than left to
	// be a replacement of its own.
	std::wstring widen(std::string_view utf8)
	{
		std::wstring wide;
		wide.reserve(utf8.size());

		std::size_t index = 0;
		while (index < utf8.size())
		{
			const unsigned char lead = static_cast<unsigned char>(utf8[index]);
			const std::size_t length = sequence_length(lead);
			if (length == 0)
			{
				wide += static_cast<wchar_t>(replacement_character);
				++index;
				continue;
			}
			if (length == 1)
			{
				wide += static_cast<wchar_t>(lead);
				++index;
				continue;
			}

			char32_t code_point =
				static_cast<char32_t>(lead & (0xFF >> (length + 1)));
			std::size_t consumed = 1;
			while (consumed < length && index + consumed < utf8.size())
			{
				const unsigned char byte =
					static_cast<unsigned char>(utf8[index + consumed]);
				const bool continuation = byte >= 0x80 && byte <= 0xBF;
				if (consumed == 1 && continuation && !valid_second(lead, byte))
				{
					++consumed;
					break;
				}
				if (!continuation)
				{
					break;
				}
				code_point = (code_point << 6) | (byte & 0x3F);
				++consumed;
			}

			if (consumed == length)
			{
				append_utf16(wide, code_point);
			}
			else
			{
				wide += static_cast<wchar_t>(replacement_character);
			}
			index += consumed;
		}
		return wide;
	}
}
