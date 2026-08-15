#pragma once

#include <string>
#include <string_view>

namespace artattack
{
	// UTF-8 to UTF-16, for the boundary where narrow text meets the render
	// module's wide text API.
	//
	// The render module holds text as std::wstring, because a glyph table is
	// keyed by code unit and a narrow entry point would therefore convert on
	// the draw path - which is either an allocation per string per view per
	// frame or a buffer shared between render workers. DirectXTK, when it owned
	// the font path, chose the second: a utfBuffer on the shared SpriteFont,
	// lazily allocated and reallocated from a const method. Converting here
	// means converting once, into storage the caller owns, off the draw path.
	//
	// Content strings arrive narrow - the level files are UTF-8 - so this is
	// where they come across. Invalid UTF-8 becomes U+FFFD rather than nothing:
	// text about to be drawn should show mojibake, not vanish.
	//
	// That promise leads somewhere now. U+FFFD is in no font this engine has
	// ever been given - MakeSpriteFont's default region is the 95 characters
	// U+0020 to U+007E - so until fonts carried a stand-in glyph, the graceful
	// path here ended in the throw it was written to avoid. The resource factory
	// installs one on every font it loads, so a replacement character draws as
	// a question mark, and RenderResources::can_render is how a caller finds out
	// before it gets that far.
	std::wstring widen(std::string_view utf8);
}
