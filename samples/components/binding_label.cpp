#include "samples/components/binding_label.h"

#include "engine/render/render_resources.h"
#include "engine/render/text_encoding.h"

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace components
{
	namespace
	{
		constexpr std::size_t max_label_bytes = 256;

		bool printable_token(std::string_view token)
		{
			if (token.empty())
			{
				return false;
			}
			for (const char unit : token)
			{
				const unsigned char character = static_cast<unsigned char>(unit);
				if (character < 0x21u || character > 0x7eu)
				{
					return false;
				}
			}
			return true;
		}

		bool single_line_label(std::wstring_view label)
		{
			bool visible = false;
			for (const wchar_t unit : label)
			{
				const char32_t character = static_cast<char32_t>(unit);
				if (character < 0x20u || (character >= 0x7fu && character <= 0x9fu) ||
					(character >= 0xd800u && character <= 0xdfffu) ||
					character == 0x2028u || character == 0x2029u)
				{
					return false;
				}
				visible = visible || character != U' ';
			}
			return visible;
		}
	}

	BindingKeyLabel binding_key_label(std::optional<std::string_view> label,
		std::string_view stable_token, const labrador::RenderResources& resources,
		labrador::FontHandle font, float max_width)
	{
		if (!std::isfinite(max_width) || max_width <= 0.0f ||
			!printable_token(stable_token))
		{
			throw std::invalid_argument("Binding label needs a positive finite width and a printable ASCII token.");
		}
		std::wstring fallback = labrador::widen(stable_token);
		if (!resources.can_render(font, fallback) ||
			resources.measure_text(font, fallback).x > max_width)
		{
			throw std::invalid_argument("Binding label font/width cannot show its stable-token fallback.");
		}
		if (!label.has_value() || label->empty())
		{
			return { std::move(fallback), BindingLabelSource::token_unavailable };
		}
		if (label->size() > max_label_bytes)
		{
			return { std::move(fallback), BindingLabelSource::token_width };
		}
		std::wstring wide = labrador::widen(*label);
		if (labrador::narrow(wide) != *label || !single_line_label(wide) ||
			!resources.can_render(font, wide))
		{
			return { std::move(fallback), BindingLabelSource::token_font };
		}
		if (resources.measure_text(font, wide).x > max_width)
		{
			return { std::move(fallback), BindingLabelSource::token_width };
		}
		return { std::move(wide), BindingLabelSource::layout };
	}
}
