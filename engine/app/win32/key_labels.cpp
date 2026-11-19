#include "engine/app/win32/key_labels.h"

#include "engine/app/win32/keyboard_input.h"
#include "engine/render/text_encoding.h"

#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

namespace labrador
{
	namespace
	{
		constexpr unsigned int scan_to_virtual_key = 3;
		constexpr unsigned int preserve_keyboard_state = 4;
		constexpr int character_capacity = 16;
		constexpr int name_capacity = 128;

		struct KeyPosition
		{
			unsigned int scan = 0;
			bool extended = false;
		};

		std::int32_t message_bits(const KeyPosition& position)
		{
			return static_cast<std::int32_t>((position.scan << 16) |
				(position.extended ? 1u << 24 : 0u));
		}

		std::optional<KeyPosition> position_of(Key key)
		{
			// These two share a set-1 scan and are distinguished by the
			// virtual-key aliases in key_from_message. Their name queries use
			// the ordinary Pause and extended NumLock forms respectively.
			if (key == Key::pause) return KeyPosition{ 0x45, false };
			if (key == Key::num_lock) return KeyPosition{ 0x45, true };

			// Reverse the existing position translator rather than duplicating
			// its key table. Ordinary forms come first, making left modifiers
			// and main Enter canonical for the engine's aggregate identities.
			for (const bool extended : { false, true })
			{
				for (unsigned int scan = 1; scan <= 0xffu; ++scan)
				{
					const KeyPosition position{ scan, extended };
					if (key_from_message(0, message_bits(position)) == key)
					{
						return position;
					}
				}
			}
			return std::nullopt;
		}

		bool character_key(Key key)
		{
			return (key >= Key::a && key <= Key::z) ||
				(key >= Key::digit_0 && key <= Key::digit_9) ||
				(key >= Key::minus && key <= Key::slash) ||
				key == Key::international_backslash;
		}

		bool high_surrogate(char32_t unit)
		{
			return unit >= 0xd800 && unit <= 0xdbff;
		}

		bool low_surrogate(char32_t unit)
		{
			return unit >= 0xdc00 && unit <= 0xdfff;
		}

		bool control(char32_t point)
		{
			return point < 0x20 || (point >= 0x7f && point <= 0x9f) ||
				point == 0x2028 || point == 0x2029;
		}

		std::string label_text(std::u16string_view text, bool single_scalar)
		{
			std::wstring wide;
			wide.reserve(text.size());
			std::size_t scalar_count = 0;
			bool visible = false;
			for (std::size_t index = 0; index < text.size(); ++index)
			{
				const char32_t unit = text[index];
				char32_t point = unit;
				if (high_surrogate(unit))
				{
					if (index + 1 >= text.size() || !low_surrogate(text[index + 1]))
					{
						return {};
					}
					point = 0x10000 + ((unit - 0xd800) << 10) +
						(text[index + 1] - 0xdc00);
					wide += static_cast<wchar_t>(unit);
					wide += static_cast<wchar_t>(text[++index]);
				}
				else if (low_surrogate(unit))
				{
					return {};
				}
				else
				{
					wide += static_cast<wchar_t>(unit);
				}
				if (control(point)) return {};
				visible = visible || point != U' ';
				++scalar_count;
			}
			if (!visible || (single_scalar && scalar_count != 1)) return {};
			return narrow(wide);
		}

		unsigned int sdk_map_virtual_key(void* /*context*/, unsigned int scan,
			unsigned int map_type, std::uintptr_t layout)
		{
			return MapVirtualKeyExW(scan, map_type, reinterpret_cast<HKL>(layout));
		}

		int sdk_to_unicode(void* /*context*/, unsigned int virtual_key,
			unsigned int scan, const unsigned char* state, char16_t* output,
			int capacity, unsigned int flags, std::uintptr_t layout)
		{
			if (capacity < 1 || capacity > character_capacity) return 0;
			std::array<wchar_t, character_capacity> wide{};
			const int count = ToUnicodeEx(virtual_key, scan, state, wide.data(),
				capacity, flags, reinterpret_cast<HKL>(layout));
			for (int index = 0; index < capacity; ++index)
			{
				output[index] = static_cast<char16_t>(wide[index]);
			}
			return count;
		}

		int sdk_key_name(void* /*context*/, std::int32_t bits, char16_t* output,
			int capacity)
		{
			if (capacity < 1 || capacity > name_capacity) return 0;
			std::array<wchar_t, name_capacity> wide{};
			const int count = GetKeyNameTextW(bits, wide.data(), capacity);
			for (int index = 0; index < capacity; ++index)
			{
				output[index] = static_cast<char16_t>(wide[index]);
			}
			return count;
		}
	}

	std::string key_label_from_layout(Key key, std::uintptr_t layout)
	{
		const NativeKeyLabelApi api{ nullptr, sdk_map_virtual_key,
			sdk_to_unicode, sdk_key_name };
		return key_label_from_layout(key, layout, api);
	}

	std::string key_label_from_layout(Key key, std::uintptr_t layout,
		const NativeKeyLabelApi& api)
	{
		if (layout == 0 || key <= Key::none || key >= Key::count) return {};
		const std::optional<KeyPosition> position = position_of(key);
		if (!position) return {};

		if (character_key(key))
		{
			if (api.map_virtual_key == nullptr || api.to_unicode == nullptr) return {};
			const unsigned int virtual_key = api.map_virtual_key(api.context,
				position->scan, scan_to_virtual_key, layout);
			if (virtual_key == 0) return {};
			const std::array<unsigned char, 256> neutral{};
			std::array<char16_t, character_capacity> output{};
			const int count = api.to_unicode(api.context, virtual_key,
				position->scan, neutral.data(), output.data(), character_capacity,
				preserve_keyboard_state, layout);
			if (count < 1 || count > 2) return {};
			return label_text(std::u16string_view(output.data(),
				static_cast<std::size_t>(count)), true);
		}

		if (api.key_name == nullptr) return {};
		std::array<char16_t, name_capacity> output{};
		const std::int32_t bits = message_bits(*position) | (1 << 25);
		const int count = api.key_name(api.context, bits, output.data(), name_capacity);
		// A full final slot can mean truncation. Do not publish a clipped name.
		if (count < 1 || count >= name_capacity - 1 || output[count] != 0) return {};
		return label_text(std::u16string_view(output.data(),
			static_cast<std::size_t>(count)), false);
	}
}
