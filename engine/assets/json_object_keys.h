#pragma once

#include "engine/assets/json.h"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Fixed-schema checks private to the manifest, sprite-sheet and sound-bank
// loaders.
// JsonValue itself permits caller-owned dictionaries and arbitrary metadata.
namespace labrador::detail
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
				{
					label += '\\';
				}
				label += character;
			}
		}
		return label;
	}

	// Refuses a key outside `allowed`, and a key written twice: the accessors
	// read the first of two, so a repeated key is a hand edit the file would
	// otherwise disagree with in silence, as an unknown one is.
	inline void reject_unknown_keys(const JsonValue& object,
		std::initializer_list<std::string_view> allowed)
	{
		const std::vector<std::string> keys = object.keys();
		for (size_t index = 0; index < keys.size(); ++index)
		{
			const std::string& key = keys[index];
			if (std::find(allowed.begin(), allowed.end(), std::string_view(key)) == allowed.end())
			{
				throw std::runtime_error(object.where() + " has unknown key '" +
					json_key_label(key) + "'.");
			}
			if (std::find(keys.begin(), keys.begin() + static_cast<std::ptrdiff_t>(index),
				key) != keys.begin() + static_cast<std::ptrdiff_t>(index))
			{
				throw std::runtime_error(object.where() + " repeats key '" +
					json_key_label(key) + "'.");
			}
		}
	}
}
