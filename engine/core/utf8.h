#pragma once

#include <string>

namespace labrador
{
	// Appends `code_point` to `text` as UTF-8, in one to four bytes.
	//
	// A surrogate (U+D800 to U+DFFF) or a value past U+10FFFF is not a
	// character, so it is appended as U+FFFD REPLACEMENT CHARACTER: one that
	// arrived broken stays visible as broken rather than silently missing.
	// The keyboard's typed text and render/text_encoding.h's narrowing share
	// this one encoder, so the two cannot disagree about a byte.
	void append_utf8(std::string& text, char32_t code_point);
}
