#pragma once

#include "engine/render/font.h"
#include "engine/render/texture_data.h"

#include <string>
#include <vector>

namespace labrador
{
	// A .spritefont, taken apart into the half that is engine data and the half
	// that needs a device.
	//
	// THE FILE IS A GLYPH TABLE WITH A TEXTURE STUCK ON THE END, which is why
	// reading it is not a backend's job even though what comes out of it is
	// half a texture. MakeSpriteFont writes a magic, the glyphs, the line
	// spacing, a default character and then an atlas as raw bytes and a
	// DXGI_FORMAT - and every one of those but the last two lines is arithmetic
	// this engine now owns (font.h). Parsing it in the backend would mean every
	// backend parsing it, and disagreeing about it silently.
	//
	// IT LIVES IN render/ AND NOT IN assets/, which is where files are read,
	// because its caller is the resource factory - which is backend code, and
	// nothing may point at assets. The same rule already moved the factory
	// itself out of assets/ (resource_factory.h).
	//
	// THE PIXELS ARE A COPY AND THE STRUCT IS RETURNED BY VALUE. This is a load
	// path, it runs once per font, and an atlas is a few tens of kilobytes; a
	// span into a buffer the caller has to keep alive would be three lines
	// faster and a lifetime rule nobody would read.
	struct SpriteFontFile
	{
		std::vector<Glyph> glyphs;
		float line_spacing = 0.0f;

		// The character the font would like drawn in place of one it lacks, or
		// 0 for "no opinion" - which is what MakeSpriteFont writes when nobody
		// chooses, and therefore what every .spritefont in this tree says.
		char32_t stand_in = 0;

		// The atlas, in the same shape a .dds comes out in - one level, since
		// the format has no way to carry a chain. Sharing the type with the
		// other reader is what lets the backend have one function that turns
		// bytes into a texture rather than one per file kind.
		TextureData atlas;
	};

	// Reads the .spritefont at `path`.
	//
	// Throws std::out_of_range naming the path if there is no file there, and
	// std::runtime_error naming the path AND what is wrong with it if there is
	// one and it is not a .spritefont this engine can read (T6). Those used to
	// be the same throw with the same message - a font that existed and was
	// corrupt reported itself as missing.
	SpriteFontFile read_sprite_font_file(const std::string& path);
}
