#include <doctest/doctest.h>

#include "samples/components/binding_label.h"

#include "engine/math/rectanglei.h"
#include "engine/render/font.h"
#include "engine/render/render_resources.h"

#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using components::BindingKeyLabel;
using components::BindingLabelSource;
using components::binding_key_label;
using namespace labrador;

namespace
{
	class LabelContent
	{
	public:
		explicit LabelContent(bool accented = true, bool fallback_glyph = true)
		{
			std::vector<Glyph> glyphs;
			for (char32_t character = U' '; character <= U'~'; ++character)
			{
				if (character == U'r' && !fallback_glyph)
				{
					continue;
				}
				Glyph glyph;
				glyph.character = character;
				glyph.subrect = RectangleI(0, 0, 10, 10);
				glyphs.push_back(glyph);
			}
			if (accented)
			{
				Glyph glyph;
				glyph.character = U'\u00e9';
				glyph.subrect = RectangleI(0, 0, 10, 10);
				glyphs.push_back(glyph);
			}
			std::unique_ptr<Font> data = std::make_unique<Font>(
				TextureHandle(), std::move(glyphs), 20.0f);
			data->set_stand_in(U'?');
			this->resources.add_font("label_font", std::move(data));
			this->font = this->resources.resolve_sprite_font("label_font");
		}

		RenderResources resources;
		FontHandle font;
	};
}

TEST_CASE("binding label uses a renderable Unicode fact without changing the stable token")
{
	LabelContent content;
	std::string label = "\xc3\xa9";
	std::string token = "r";
	const BindingKeyLabel displayed = binding_key_label(label, token,
		content.resources, content.font, 20.0f);
	CHECK(displayed.text == L"\u00e9");
	CHECK(displayed.source == BindingLabelSource::layout);
	CHECK(label == "\xc3\xa9");
	CHECK(token == "r");
	label.clear();
	token.clear();
	CHECK(displayed.text == L"\u00e9");
}

TEST_CASE("missing binding label uses the exact supplied stable token")
{
	LabelContent content;
	for (const std::optional<std::string_view> label :
		{ std::optional<std::string_view>(), std::optional<std::string_view>("") })
	{
		const BindingKeyLabel displayed = binding_key_label(label, "digit_9",
			content.resources, content.font, 100.0f);
		CHECK(displayed.text == L"digit_9");
		CHECK(displayed.source == BindingLabelSource::token_unavailable);
	}
}

TEST_CASE("a font stand-in does not substitute for a binding label's missing glyph")
{
	LabelContent content(false);
	REQUIRE(content.resources.font(content.font)->drawn(U'\u00e9').character == U'?');
	const BindingKeyLabel displayed = binding_key_label("\xc3\xa9", "r",
		content.resources, content.font, 20.0f);
	CHECK(displayed.text == L"r");
	CHECK(displayed.source == BindingLabelSource::token_font);
}

TEST_CASE("malformed and non-single-line binding labels use the stable token")
{
	LabelContent content;
	const std::vector<std::string_view> labels{
		"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xc3", "\n",
		"\t", "R\nX", "\x7f", "\xc2\x85", "\xe2\x80\xa8", "\xe2\x80\xa9",
		"\xf0\x9f\x98\x80", " ", std::string_view("r\0x", 3)
	};
	for (const std::string_view label : labels)
	{
		const BindingKeyLabel displayed = binding_key_label(label, "r",
			content.resources, content.font, 100.0f);
		CHECK(displayed.text == L"r");
		CHECK(displayed.source == BindingLabelSource::token_font);
	}
}

TEST_CASE("binding labels fall back before exceeding the row width or conversion bound")
{
	LabelContent content;
	const BindingKeyLabel exact = binding_key_label("XX", "r",
		content.resources, content.font, 20.0f);
	CHECK(exact.text == L"XX");
	CHECK(exact.source == BindingLabelSource::layout);
	const BindingKeyLabel narrow = binding_key_label("XXX", "r",
		content.resources, content.font, 20.0f);
	CHECK(narrow.text == L"r");
	CHECK(narrow.source == BindingLabelSource::token_width);
	const std::string oversize(257, 'X');
	const BindingKeyLabel bounded = binding_key_label(oversize, "r",
		content.resources, content.font, 3000.0f);
	CHECK(bounded.text == L"r");
	CHECK(bounded.source == BindingLabelSource::token_width);
}

TEST_CASE("a binding label requires a renderable fitting stable-token fallback")
{
	LabelContent missing(false, false);
	CHECK_THROWS_AS(binding_key_label("Q", "r", missing.resources,
		missing.font, 100.0f), std::invalid_argument);
	LabelContent content;
	CHECK_THROWS_AS(binding_key_label("Q", "digit_9", content.resources,
		content.font, 20.0f), std::invalid_argument);
	for (const float width : { 0.0f, -1.0f,
		std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::quiet_NaN() })
	{
		CHECK_THROWS_AS(binding_key_label("Q", "r", content.resources,
			content.font, width), std::invalid_argument);
	}
	for (const std::string_view token : { "", "r r", "\xc3\xa9", "r\n" })
	{
		CHECK_THROWS_AS(binding_key_label("Q", token, content.resources,
			content.font, 100.0f), std::invalid_argument);
	}
}
