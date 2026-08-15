#include "engine/render/font.h"

#include <algorithm>
#include <stdexcept>
#include <string>

using namespace mattmath;

namespace artattack
{
	namespace
	{
		// A character in a message, without assuming it is printable. The
		// commonest one to report is exactly the one a terminal cannot show.
		std::string describe(char32_t character)
		{
			const char* digits = "0123456789ABCDEF";
			const unsigned int value = static_cast<unsigned int>(character);

			std::string text;
			// Four digits at least, which is how a code point is written, and
			// more only when the character needs them.
			for (int shift = 28; shift >= 0; shift -= 4)
			{
				const unsigned int nibble =
					(value >> shift) & 0xFu;
				if (text.empty() && nibble == 0 && shift >= 16)
				{
					continue;
				}
				text += digits[nibble];
			}
			return "U+" + text;
		}
	}

	Font::Font(TextureHandle atlas, std::vector<Glyph> glyphs,
		float line_spacing)
		: glyphs_(std::move(glyphs)), atlas_(atlas),
		line_spacing_(line_spacing)
	{
		std::sort(this->glyphs_.begin(), this->glyphs_.end(),
			[](const Glyph& left, const Glyph& right)
			{
				return left.character < right.character;
			});
	}

	const Glyph* Font::find(char32_t character) const
	{
		// A binary search, which is what makes the lookup affordable on the
		// frame path: a HUD line is a few dozen of these and an atlas is a
		// hundred glyphs, so the alternative is a few thousand comparisons per
		// string per view per frame.
		const std::vector<Glyph>::const_iterator found = std::lower_bound(
			this->glyphs_.begin(), this->glyphs_.end(), character,
			[](const Glyph& glyph, char32_t sought)
			{
				return glyph.character < sought;
			});

		if (found == this->glyphs_.end() || found->character != character)
		{
			return nullptr;
		}
		return &*found;
	}

	const Glyph& Font::drawn(char32_t character) const
	{
		const Glyph* glyph = this->find(character);
		if (glyph != nullptr)
		{
			return *glyph;
		}

		if (this->stand_in_ >= 0)
		{
			return this->glyphs_[static_cast<size_t>(this->stand_in_)];
		}

		// Reachable only for a font nobody installed a stand-in on, which the
		// resource factory does to every font it loads that has a candidate.
		// An atlas with neither a question mark nor a space is not a text font.
		throw std::runtime_error("This font has no glyph for " +
			describe(character) + " and no stand-in to draw instead.");
	}

	void Font::set_stand_in(char32_t character)
	{
		const Glyph* glyph = this->find(character);
		if (glyph == nullptr)
		{
			throw std::out_of_range("This font has no glyph for " +
				describe(character) + ", so it cannot stand in for one.");
		}
		this->stand_in_ =
			static_cast<int>(glyph - this->glyphs_.data());
	}

	Vector2F Font::measure(std::wstring_view text) const
	{
		Vector2F extent = Vector2F::ZERO;

		this->for_each_glyph(text,
			[&extent, this](const Glyph& glyph, const Vector2F& pen)
			{
				const float width = static_cast<float>(glyph.subrect.width);

				// A LINE IS AT LEAST A LINE TALL. Measuring a line of full
				// stops as four pixels high would collapse every layout that
				// stacks measured text, so the glyph's own extent only wins
				// when it is the taller of the two. Whitespace wide enough to
				// be reported at all contributes the line spacing and nothing
				// of its own - it has no ink to be tall.
				float height = static_cast<float>(glyph.subrect.height) +
					glyph.y_offset;
				height = is_blank(glyph.character)
					? this->line_spacing_
					: (height > this->line_spacing_ ? height
						: this->line_spacing_);

				const float right = pen.x + width;
				const float bottom = pen.y + height;
				if (right > extent.x)
				{
					extent.x = right;
				}
				if (bottom > extent.y)
				{
					extent.y = bottom;
				}
			});

		return extent;
	}

	size_t Font::first_unrenderable(std::wstring_view text) const
	{
		for (size_t i = 0; i < text.size(); i++)
		{
			if (this->find(static_cast<char32_t>(text[i])) == nullptr)
			{
				return i;
			}
		}
		return std::wstring_view::npos;
	}
}
