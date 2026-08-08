#include "engine/render/text_encoding.h"

#include <Windows.h>

namespace artattack
{
	std::wstring widen(std::string_view utf8)
	{
		if (utf8.empty())
		{
			return {};
		}

		const int narrow_length = static_cast<int>(utf8.size());
		const int wide_length = MultiByteToWideChar(
			CP_UTF8, 0, utf8.data(), narrow_length, nullptr, 0);
		if (wide_length <= 0)
		{
			return {};
		}

		std::wstring wide(static_cast<size_t>(wide_length), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8.data(), narrow_length,
			wide.data(), wide_length);
		return wide;
	}
}
