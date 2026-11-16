#pragma once

#include "engine/render/render_resources.h"
#include "engine/ui/widget.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace components
{
	struct TextFieldLimits
	{
		std::size_t max_code_points = 12;
		// Unscaled font texels, including the end caret even while not editing.
		float max_width = 160.0f;
	};

	enum class TextEdit
	{
		accepted,
		unchanged,
		inactive,
		invalid_utf8,
		unsupported_character,
		missing_glyph,
		length_limit,
		width_limit
	};

	// A single-line field a game copies and edits. The page owns focus and
	// bindings: begin_edit on activation, feed typed UTF-8 and explicit editing
	// commands, and cancel on focus loss. No device is polled here.
	//
	// Storage stays valid UTF-8. Editing appends at an end caret and backspace
	// removes one code point, not a grapheme. Controls, Unicode line separators,
	// non-BMP characters and missing glyphs are refused without substitution.
	// Glyph coverage does not add shaping, combining placement or bidi: the
	// language limits of engine/render/font.h still apply.
	class TextField final : public labrador::UiContainer
	{
	public:
		// The resource table is borrowed and must outlive the field. Font
		// definitions/metrics stay unchanged; texture/context reload is allowed.
		// The font must contain the label, initial value and an ASCII '|'
		// cell with positive width and height. Invalid
		// limits or initial content throw std::invalid_argument naming the field.
		TextField(const std::string& name, const std::wstring& label,
			std::string value, TextFieldLimits limits,
			const labrador::Vector2F& label_position, float value_x,
			const std::string& font_name, labrador::RenderResources* resources);

		// The container borrows its own child members.
		TextField(const TextField&) = delete;
		TextField& operator=(const TextField&) = delete;

		// A repeated begin does not reset a draft. The page ignores the input
		// snapshot that opened editing, so its Enter/Space cannot also edit it.
		bool begin_edit();
		// A complete batch is accepted or refused; no fitting prefix is kept.
		// Inactive input is ignored, including malformed input. An empty batch
		// changes nothing. State is bounded by the configured code-point limit.
		TextEdit append(std::string_view utf8);
		bool backspace();
		// Confirm ends editing and returns whether the committed value changed.
		// Cancel ends editing and restores the prior value; true means it was
		// editing. Neither changes an inactive field.
		bool confirm();
		bool cancel();

		bool editing() const;
		const std::string& value() const;
		const std::string& draft() const;
		std::size_t code_points() const;
		const labrador::UiText& label() const;
		const labrador::UiText& display() const;
		void set_colour(const labrador::Colour& colour) override;

	private:
		TextEdit check_text(std::string_view utf8, std::wstring& wide) const;
		void refresh_display();

		labrador::RenderResources* resources_;
		labrador::FontHandle font_;
		TextFieldLimits limits_;
		labrador::UiText label_;
		labrador::UiText display_;
		std::string value_;
		std::string draft_;
		std::wstring wide_;
		bool editing_ = false;
	};
}
