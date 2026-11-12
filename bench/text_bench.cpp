#include "bench/bench.h"

#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/font.h"
#include "engine/render/renderer.h"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Text layout's arithmetic: the glyph walk and the word wrap.
//
// Font::for_each_glyph is the one piece of engine/render/font.h the frame path
// walks, because a HUD measures or draws every string it shows every frame,
// and Font::wrap states that it visits each unit at most twice
// (engine/render/font.cpp). Both are headless engine data over a texture
// handle that is never resolved, so like render_bench.cpp this builds and runs
// under all five render backends and creates no device.
//
// The walk is timed through Font::measure, its out-of-line caller: every
// draw_text call walks the same pen, and measure adds only a comparison per
// glyph to it.
//
// THE OBJECT IN THE "ns/object" COLUMN IS ONE UTF-16 UNIT OF THE TEXT. The
// glyph table stays the same size at every count, so its binary search is a
// constant per unit; what these rows guard is a change that makes one unit's
// cost depend on how long the string is.

using labrador::Font;
using labrador::Glyph;
using labrador::RectangleI;
using labrador::TextureHandle;
using labrador::Vector2F;

namespace
{
	// Every letter is eleven texels wide with an advance adjustment of -1, so
	// the pen steps ten a letter; the negative adjustment is what most glyphs
	// in the template's atlas have (font.h's file header).
	constexpr int letter_width = 11;
	constexpr float letter_step = 10.0f;

	// The space is the one-texel glyph MakeSpriteFont writes, which steps the
	// pen and draws nothing.
	constexpr float space_step = 4.0f;

	constexpr int word_letters = 5;

	// A line of one word, the space and the next word's first four letters
	// ends at 95 texels and fits; the fifth letter ends it at 105, which does
	// not. So the wrap breaks at every space only on a word's last letter,
	// and every word after the first is walked twice in full: once until it
	// overflows, and once replayed from the start of its new line. That is
	// the worst case the "at most twice" bound in font.cpp names.
	constexpr float wrap_width =
		static_cast<float>(2 * word_letters) * letter_step;

	// The printable ASCII range, so the glyph lookup searches a table the
	// size a real font's is rather than one of three entries.
	Font bench_font()
	{
		std::vector<Glyph> glyphs;
		for (char32_t character = U'!'; character <= U'~'; character++)
		{
			Glyph glyph;
			glyph.character = character;
			glyph.subrect = RectangleI(0, 0, letter_width, 16);
			glyph.x_advance = letter_step - static_cast<float>(letter_width);
			glyphs.push_back(glyph);
		}

		Glyph space;
		space.character = U' ';
		space.subrect = RectangleI(0, 0, 1, 1);
		space.x_advance = space_step - 1.0f;
		glyphs.push_back(space);

		return Font(TextureHandle(), std::move(glyphs), 20.0f);
	}

	// `count` units of five-letter words separated by single spaces, the
	// letters cycling through the alphabet so neighbouring lookups differ.
	std::wstring words(int count)
	{
		std::wstring text;
		text.reserve(static_cast<size_t>(count));

		int letter = 0;
		while (static_cast<int>(text.size()) < count)
		{
			const int position = static_cast<int>(text.size()) %
				(word_letters + 1);
			if (position == word_letters)
			{
				text.push_back(L' ');
			}
			else
			{
				text.push_back(static_cast<wchar_t>(L'a' + letter % 26));
				letter++;
			}
		}
		return text;
	}

	size_t count_line_feeds(const std::wstring& text)
	{
		size_t feeds = 0;
		for (wchar_t unit : text)
		{
			if (unit == L'\n')
			{
				feeds++;
			}
		}
		return feeds;
	}

	// The spaces a whole word follows. The ladder's counts leave a shorter
	// word at the end, which fits after its space and so breaks nothing.
	size_t count_spaces_before_whole_words(const std::wstring& text)
	{
		const size_t whole = static_cast<size_t>(word_letters);
		size_t spaces = 0;
		for (size_t i = 0; i < text.size(); i++)
		{
			if (text[i] == L' ' && i + whole < text.size())
			{
				spaces++;
			}
		}
		return spaces;
	}

	// Keeps both results from being dead stores, as render_bench.cpp's sink
	// does for its vertices.
	volatile float measured_sink = 0.0f;
	volatile size_t wrapped_sink = 0;
}

void run_text_benchmarks()
{
	const Font font = bench_font();

	// The ladder render_bench.cpp uses, and for its reason: the span is 64x,
	// which is what main.cpp's 8x slack was chosen against. The smallest is
	// longer than any one string a HUD shows, so that the table's lookup and
	// the call itself do not dominate the per-unit figure.
	const int counts[] = { 1024, 4096, 16384, 65536 };

	for (int count : counts)
	{
		const std::wstring text = words(count);

		// The row would measure something other than its name if a change to
		// the font or the widths above stopped every whole word from breaking.
		if (count_line_feeds(font.wrap(text, wrap_width)) !=
			count_spaces_before_whole_words(text))
		{
			throw std::logic_error("text_bench.cpp: the wrap width no longer "
				"breaks every word, so the wrap row is not its worst case.");
		}

		bench::record(bench::run("Font::measure (glyph walk)", count,
			[&font, &text]
			{
				const Vector2F extent = font.measure(text);
				measured_sink = extent.x;
			}));

		bench::record(bench::run("Font::wrap (every word breaks)", count,
			[&font, &text]
			{
				const std::wstring wrapped = font.wrap(text, wrap_width);
				wrapped_sink = wrapped.size();
			}));
	}
}
