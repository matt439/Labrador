#pragma once

#include "engine/assets/json.h"

#include <algorithm>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>

// Fixed-schema checks private to the manifest and sprite-sheet loaders.
// JsonValue itself permits caller-owned dictionaries and arbitrary metadata.
namespace labrador::asset_details
{
	inline std::string json_key_label(const std::string& key)
	{
		constexpr char digits[] = "0123456789abcdef";
		std::string label;
		for (const char character : key)
		{
			const unsigned char byte = static_cast<unsigned char>(character);
			if (byte < 0x20 || byte == 0x7f)
			{
				label += "\\u00";
				label += digits[byte >> 4];
				label += digits[byte & 0x0f];
			}
			else
			{
				if (character == '\\' || character == '\'')
					label += '\\';
				label += character;
			}
		}
		return label;
	}

	inline void reject_unknown_keys(const JsonValue& object,
		std::initializer_list<std::string_view> allowed)
	{
		for (const std::string& key : object.keys())
		{
			if (std::find(allowed.begin(), allowed.end(), std::string_view(key)) == allowed.end())
			{
				throw std::runtime_error(object.where() + " has unknown key '" +
					json_key_label(key) + "'.");
			}
		}
	}
}
