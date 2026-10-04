#pragma once

#include <string>
#include <string_view>

namespace labrador
{
	// UTF-8 to UTF-16, for the boundary where narrow text meets the render
	// module's wide text API.
	//
	// The render module holds text as std::wstring, because a glyph table is
	// keyed by code unit and a narrow entry point would therefore convert on
	// the draw path - which is either an allocation per string per view per
	// frame or a buffer shared between render workers. Converting here means
	// converting once, into storage the caller owns, off the draw path.
	//
	// Content strings arrive narrow - content files are UTF-8 - so this is
	// where they come across. Invalid UTF-8 becomes U+FFFD rather than nothing:
	// text about to be drawn should show mojibake, not vanish.
	//
	// U+FFFD is in no font MakeSpriteFont makes by default - its default
	// region is the 95 characters U+0020 to U+007E - so what draws in its place
	// is the font's stand-in glyph. The resource factory installs one on every
	// font that HAS a candidate - the font's own choice, then '?', then ' ' -
	// so on every font in this tree a replacement character draws as a
	// question mark, and on a font carrying none of the three nothing is
	// installed and drawing one throws (Font::drawn).
	// RenderResources::can_render is how a caller finds out before it gets that
	// far.
	std::wstring widen(std::string_view utf8);
}
