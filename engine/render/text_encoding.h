#pragma once

#include <string>
#include <string_view>

namespace artattack
{
	// UTF-8 to UTF-16, for the boundary where narrow text meets the render
	// module's wide text API.
	//
	// The render module holds text as std::wstring and calls DirectXTK's wide
	// overloads. The narrow overloads look equivalent and are not: they convert
	// through a utfBuffer owned by the shared SpriteFont, lazily allocated and
	// reallocated from a const method, and the render workers all hold the same
	// SpriteFont. Converting here means converting once, into storage the caller
	// owns, off the draw path.
	//
	// Content strings arrive narrow - the level files are UTF-8 - so this is
	// where they come across. Invalid UTF-8 becomes U+FFFD rather than nothing:
	// text about to be drawn should show mojibake, not vanish.
	//
	// That promise leads somewhere now. U+FFFD is in no font this engine has
	// ever been given - MakeSpriteFont's default region is the 95 characters
	// U+0020 to U+007E - so until fonts carried a stand-in glyph, the graceful
	// path here ended in the throw it was written to avoid. The font kind
	// installs one at load (assets/resource_loader.cpp), so a replacement
	// character draws as a question mark, and RenderResources::can_render is
	// how a caller finds out before it gets that far.
	std::wstring widen(std::string_view utf8);
}
