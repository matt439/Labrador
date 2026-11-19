#pragma once

#include "engine/render/renderer.h"

#include <optional>
#include <string>
#include <string_view>

namespace labrador
{
	class RenderResources;
}

namespace components
{
	enum class BindingLabelSource
	{
		layout, token_unavailable, token_font, token_width,
	};

	struct BindingKeyLabel
	{
		std::wstring text;
		BindingLabelSource source = BindingLabelSource::token_unavailable;
	};

	// Copied page policy: show a supplied UTF-8 label only when it is valid,
	// single-line text this font can draw within the row's width. Otherwise
	// show the supplied stable token; neither string changes a binding.
	// Labels over 256 bytes take the width fallback before conversion.
	// The result owns its wide text. Cache it on label/binding changes, not
	// during draw(). The fallback token must be nonempty printable ASCII,
	// renderable and within a finite positive max_width, or this throws
	// std::invalid_argument: the page must provide a usable fallback route.
	BindingKeyLabel binding_key_label(std::optional<std::string_view> label,
		std::string_view stable_token, const labrador::RenderResources& resources,
		labrador::FontHandle font, float max_width);
}
