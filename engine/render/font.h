#pragma once

#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/renderer.h"

#include <cstddef>
#include <string_view>
#include <vector>

// A font, as engine data.
//
// WHAT CHANGED AND WHY IT MATTERS. A Font used to be one of the two phantom
// types in renderer.h - a name with no definition, whose meaning the backend
// chose, which in practice meant DirectX::SpriteFont. That put the pen
// arithmetic, the glyph table, the line spacing and the measurement of every
// string this engine draws inside a library only one backend can link. A
// second backend would have had to find or write all of it again, and would
// have had no way to check that its answers matched, because the answers were
// nowhere written down.
//
// So a Font is now what a SpriteSheet already is: engine data over a
// TextureHandle. The atlas is still the backend's - it is a texture, and only
// the backend knows what one of those is - but where each glyph sits in it,
// how far the pen moves and how tall a line is are the engine's, in this file,
// in arithmetic a backend cannot get wrong because it never performs any of
// it. Texture remains phantom; Font does not.
//
// THE ARITHMETIC IS DirectXTK'S, DELIBERATELY AND EXACTLY. Every quirk below
// was SpriteFont::Impl::ForEachGlyph's, and the commit that moved it here
// changed none of them, because RenderPixelTests pins what they produce and
// the point of that commit was that the pixels did not move. They are written
// down here rather than merely reproduced, so the next person to simplify one
// is doing it on purpose:
//
//   - x_advance is an ADJUSTMENT to the advance, not the advance. The pen
//     moves by the glyph's width plus x_advance, so a negative value - which
//     most glyphs in a Courier atlas have - is normal and not corruption.
//   - x_offset is a left bearing applied before the glyph is drawn and after
//     the pen has already moved, so it does not accumulate.
//   - a whitespace glyph no larger than one texel in both axes steps the pen
//     and draws nothing. MakeSpriteFont writes exactly such a glyph for U+0020,
//     and it is not required to be transparent.
//   - a line is the taller of the glyph's own extent and the font's line
//     spacing, so a line of full stops measures as tall as a line of capitals.
//
// WHAT WAS DROPPED, AND IT IS ONE THING. SpriteFont's walk took an
// ignoreWhitespace flag; both callers in this engine passed the same value, so
// there is no flag (T3). MeasureDrawBounds, the third caller upstream that
// wanted the other value, has never existed here.

namespace labrador
{
	// One character's cell in a font's atlas.
	//
	// Public fields and no accessors: this is a record read straight out of a
	// file, and the four numbers beside the rectangle are the only reason a
	// glyph is not just a rectangle.
	struct Glyph
	{
		char32_t character = 0;

		// Where in the atlas, in texels.
		mattmath::RectangleI subrect;

		// Added to the pen before drawing, and not carried forward.
		float x_offset = 0.0f;

		// Added to the line's y before drawing, and not carried forward.
		float y_offset = 0.0f;

		// Added to the glyph's width to make the step to the next pen
		// position. Usually negative - see the file header.
		float x_advance = 0.0f;
	};

	class Font
	{
	public:
		// `glyphs` need not be sorted; this sorts them, because the lookup is a
		// binary search and a file is not obliged to be in order.
		Font(TextureHandle atlas, std::vector<Glyph> glyphs,
			float line_spacing);

		// The texture every glyph is cut from. A handle, so a device loss that
		// empties the slot and a reload that refills it are invisible here -
		// which is the whole reason a Font is no longer a device resource.
		TextureHandle atlas() const { return this->atlas_; }

		float line_spacing() const { return this->line_spacing_; }

		// The glyph the font has for `character`, with no substitution, or
		// nullptr. This is the question can_render asks.
		const Glyph* find(char32_t character) const;

		bool contains(char32_t character) const
		{
			return this->find(character) != nullptr;
		}

		// The glyph to draw for `character`: its own, or the stand-in.
		//
		// Throws std::runtime_error naming the character when the font has
		// neither - which is a font with no stand-in installed, and means an
		// atlas that is not a text font at all.
		const Glyph& drawn(char32_t character) const;

		// What `drawn` falls back to. Throws std::out_of_range naming the
		// character if the font has no glyph for it, which is the throw the
		// stand-in exists to prevent and so must not itself cause.
		void set_stand_in(char32_t character);

		// The unscaled extent of `text`: the width of the longest line, and the
		// bottom of the lowest one.
		mattmath::Vector2F measure(std::wstring_view text) const;

		// Where the first character with no glyph of its own is, or
		// std::wstring_view::npos. Per UTF-16 unit, so a character outside the
		// basic plane is two and reports at the first of them.
		size_t first_unrenderable(std::wstring_view text) const;

		// The pen walk both of the above and every draw_text share.
		//
		// `action` is called as action(glyph, pen) for each glyph that is to be
		// drawn, where pen.x is the pen position with the glyph's left bearing
		// already added and pen.y is the TOP OF THE LINE - the glyph's own
		// y_offset is not in it, because measurement and drawing want it at
		// different moments. Whitespace that draws nothing is not reported.
		//
		// A template, and therefore in the header, because it is the one piece
		// of this file the frame path walks: a HUD redraws every string it
		// shows every frame, and an out-of-line call plus an indirection per
		// glyph is a cost T8 does not ask anyone to pay for a callback that is
		// always one of two lambdas.
		template <typename Action>
		void for_each_glyph(std::wstring_view text, Action action) const
		{
			float x = 0.0f;
			float y = 0.0f;

			for (size_t i = 0; i < text.size(); i++)
			{
				const char32_t character =
					static_cast<char32_t>(text[i]);

				if (character == U'\r')
				{
					continue;
				}
				if (character == U'\n')
				{
					x = 0.0f;
					y += this->line_spacing_;
					continue;
				}

				const Glyph& glyph = this->drawn(character);

				x += glyph.x_offset;
				if (x < 0.0f)
				{
					x = 0.0f;
				}

				// A space steps the pen and marks nothing. The test is on the
				// character asked for rather than on the glyph found, so a tab
				// no font has draws the stand-in - a question mark is wider
				// than a texel, and the stand-in is meant to be seen.
				if (!is_blank(character) || glyph.subrect.width > 1 ||
					glyph.subrect.height > 1)
				{
					action(glyph, mattmath::Vector2F(x, y));
				}

				x += static_cast<float>(glyph.subrect.width) + glyph.x_advance;
			}
		}

	private:
		// The whitespace the walk skips.
		//
		// SPELT OUT RATHER THAN ASKED OF <cwctype>. SpriteFont called iswspace,
		// which answers by the process's current C locale - so which characters
		// stepped the pen without drawing depended on whether anything in the
		// process had ever called setlocale, and a client that localised its
		// number formatting could move its own text. Nothing in this tree calls
		// setlocale, so this set is exactly what iswspace answered here; it is
		// now what it answers everywhere.
		static bool is_blank(char32_t character)
		{
			return character == U' ' || character == U'\t' ||
				character == U'\v' || character == U'\f' ||
				character == U'\n' || character == U'\r';
		}

		std::vector<Glyph> glyphs_;
		TextureHandle atlas_;
		float line_spacing_ = 0.0f;

		// Index into glyphs_, or -1. Not a pointer: the vector is sorted in the
		// constructor and never changes afterwards, but an index says plainly
		// that this refers to a slot in it.
		int stand_in_ = -1;
	};
}
