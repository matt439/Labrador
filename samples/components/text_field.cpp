#include "samples/components/text_field.h"

#include "engine/render/font.h"
#include "engine/render/text_encoding.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

using namespace labrador;

namespace components
{
	namespace
	{
		constexpr Colour value_colour(235, 190, 90);
		constexpr Colour editing_colour(180, 230, 255);

		RenderResources* checked_resources(RenderResources* resources,
			const std::string& name)
		{
			if (resources == nullptr)
			{
				throw std::invalid_argument("TextField '" + name +
					"' needs a resource table");
			}
			return resources;
		}

		FontHandle checked_font(RenderResources& resources,
			const std::string& name, const std::string& font_name,
			const std::wstring& label)
		{
			const FontHandle font = resources.resolve_sprite_font(font_name);
			const Glyph* const caret = resources.font(font)->find(U'|');
			if (!resources.can_render(font, label) || caret == nullptr ||
				caret->subrect.width <= 0 || caret->subrect.height <= 0)
			{
				throw std::invalid_argument("TextField '" + name +
					"' needs label glyphs and a drawable end caret '|'");
			}
			return font;
		}
	}

	TextField::TextField(const std::string& name, const std::wstring& label,
		std::string value, TextFieldLimits limits,
		const Vector2F& label_position, float value_x,
		const std::string& font_name, RenderResources* resources) :
		UiContainer(name),
		resources_(checked_resources(resources, name)),
		font_(checked_font(*this->resources_, name, font_name, label)),
		limits_(limits),
		label_(name + "_label", label, font_name, label_position, resources_),
		display_(name + "_display", L"", font_name,
			Vector2F(value_x, label_position.y), resources_, value_colour),
		value_(std::move(value))
	{
		if (limits.max_code_points == 0 ||
			limits.max_code_points > std::numeric_limits<std::size_t>::max() / 4 ||
			!std::isfinite(limits.max_width) || limits.max_width <= 0.0f)
		{
			throw std::invalid_argument("TextField '" + name + "' has invalid limits");
		}
		if (this->check_text(this->value_, this->wide_) != TextEdit::accepted)
		{
			throw std::invalid_argument("TextField '" + name +
				"' has unsupported initial text or exceeded limits");
		}
		this->draft_ = this->value_;
		this->refresh_display();
		this->add_child(&this->label_);
		this->add_child(&this->display_);
	}

	TextEdit TextField::check_text(std::string_view utf8, std::wstring& wide) const
	{
		// Bound conversion/allocation before looking at an arbitrary input batch.
		if (utf8.size() > this->limits_.max_code_points * 4)
		{
			return TextEdit::length_limit;
		}
		wide = widen(utf8);
		if (narrow(wide) != utf8)
		{
			return TextEdit::invalid_utf8;
		}
		for (wchar_t unit : wide)
		{
			const char32_t character = static_cast<char32_t>(unit);
			if (character < 0x20 || (character >= 0x7F && character <= 0x9F) ||
				(character >= 0xD800 && character <= 0xDFFF) ||
				character == 0x2028 || character == 0x2029)
			{
				return TextEdit::unsupported_character;
			}
		}
		if (wide.size() > this->limits_.max_code_points)
		{
			return TextEdit::length_limit;
		}
		if (!this->resources_->can_render(this->font_, wide))
		{
			return TextEdit::missing_glyph;
		}
		// A drawn caret makes trailing whitespace advance part of the measured
		// extent. measure(text).x alone is not a pen position (font.h).
		std::wstring with_caret = wide;
		with_caret.push_back(L'|');
		if (this->resources_->measure_text(this->font_, with_caret).x >
			this->limits_.max_width)
		{
			return TextEdit::width_limit;
		}
		return TextEdit::accepted;
	}

	bool TextField::begin_edit()
	{
		if (this->editing_)
		{
			return false;
		}
		this->editing_ = true;
		this->refresh_display();
		return true;
	}

	TextEdit TextField::append(std::string_view utf8)
	{
		if (!this->editing_)
		{
			return TextEdit::inactive;
		}
		if (utf8.empty())
		{
			return TextEdit::unchanged;
		}
		const std::size_t max_bytes = this->limits_.max_code_points * 4;
		if (utf8.size() > max_bytes - this->draft_.size())
		{
			return TextEdit::length_limit;
		}
		std::string candidate = this->draft_;
		candidate.append(utf8);
		std::wstring converted;
		const TextEdit result = this->check_text(candidate, converted);
		if (result != TextEdit::accepted)
		{
			return result;
		}
		this->draft_ = std::move(candidate);
		this->wide_ = std::move(converted);
		this->refresh_display();
		return TextEdit::accepted;
	}

	bool TextField::backspace()
	{
		if (!this->editing_ || this->draft_.empty())
		{
			return false;
		}
		std::size_t last = this->draft_.size() - 1;
		while (last > 0 &&
			(static_cast<unsigned char>(this->draft_[last]) & 0xC0) == 0x80)
		{
			--last;
		}
		this->draft_.erase(last);
		this->wide_ = widen(this->draft_);
		this->refresh_display();
		return true;
	}

	bool TextField::confirm()
	{
		if (!this->editing_)
		{
			return false;
		}
		const bool changed = this->value_ != this->draft_;
		this->value_ = this->draft_;
		this->editing_ = false;
		this->refresh_display();
		return changed;
	}

	bool TextField::cancel()
	{
		if (!this->editing_)
		{
			return false;
		}
		this->draft_ = this->value_;
		this->wide_ = widen(this->draft_);
		this->editing_ = false;
		this->refresh_display();
		return true;
	}

	void TextField::refresh_display()
	{
		std::wstring text = this->wide_;
		if (this->editing_)
		{
			text.push_back(L'|');
		}
		this->display_.set_text(text);
		this->display_.set_colour(this->editing_ ? editing_colour : value_colour);
	}

	bool TextField::editing() const { return this->editing_; }
	const std::string& TextField::value() const { return this->value_; }
	const std::string& TextField::draft() const { return this->draft_; }
	std::size_t TextField::code_points() const { return this->wide_.size(); }
	const UiText& TextField::label() const { return this->label_; }
	const UiText& TextField::display() const { return this->display_; }
	void TextField::set_colour(const Colour& colour) { this->label_.set_colour(colour); }
}
