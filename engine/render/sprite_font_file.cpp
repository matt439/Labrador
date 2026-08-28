#include "engine/render/sprite_font_file.h"

#include "engine/core/byte_reader.h"
#include "engine/math/rectanglei.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

using namespace mattmath;

namespace labrador
{
	namespace
	{
		const char MAGIC[] = "DXTKfont";
		const size_t MAGIC_LENGTH = 8;

		// One glyph is a character, four rectangle edges and three floats.
		const size_t GLYPH_BYTES = 32;

		// The format numbers are DXGI's, and this reader is the one place
		// outside engine/render/<backend>/ that knows any - because they are
		// not the backend's choice here, they are what is written in the file.
		// A file format that happens to be spelt in another API's constants is
		// still a file format. (dds_file.cpp is the other reader and needs no
		// DXGI number at all: a .dds says its format as a fourCC.)
		const uint32_t DXGI_R8G8B8A8_UNORM = 28;
		const uint32_t DXGI_BC2_UNORM = 74;
		const uint32_t DXGI_B4G4R4A4_UNORM = 115;

		TextureFormat to_texture_format(uint32_t dxgi_format,
			const std::string& path)
		{
			switch (dxgi_format)
			{
			case DXGI_R8G8B8A8_UNORM: return TextureFormat::r8g8b8a8_unorm;
			case DXGI_B4G4R4A4_UNORM: return TextureFormat::b4g4r4a4_unorm;
			case DXGI_BC2_UNORM:      return TextureFormat::bc2_unorm;
			default: break;
			}

			throw std::runtime_error(path + " holds its atlas in DXGI format " +
				std::to_string(dxgi_format) + ", which this engine does not "
				"read. MakeSpriteFont writes 28, 74 or 115 "
				"(texture_format.h).");
		}
	}

	SpriteFontFile read_sprite_font_file(const std::string& path)
	{
		const std::vector<unsigned char> bytes = read_file_bytes(path);
		ByteReader reader(bytes, path);

		reader.require(MAGIC_LENGTH);
		for (size_t i = 0; i < MAGIC_LENGTH; i++)
		{
			if (bytes[i] != static_cast<unsigned char>(MAGIC[i]))
			{
				throw std::runtime_error(path + " is not a MakeSpriteFont "
					"binary: it does not start with \"DXTKfont\".");
			}
		}
		reader.skip(MAGIC_LENGTH);

		SpriteFontFile font;

		const uint32_t glyph_count = reader.read_u32();
		// Checked before reserving rather than after: the count is the first
		// number in the file that a corrupt file can use to ask for gigabytes.
		reader.require(static_cast<size_t>(glyph_count) * GLYPH_BYTES);
		font.glyphs.reserve(glyph_count);

		for (uint32_t i = 0; i < glyph_count; i++)
		{
			Glyph glyph;
			glyph.character = static_cast<char32_t>(reader.read_u32());

			// The file stores a RECT - left, top, right, bottom - and a
			// RectangleI is a position and a size, which is the conversion the
			// rest of the engine would otherwise have to remember.
			const int32_t left = reader.read_i32();
			const int32_t top = reader.read_i32();
			const int32_t right = reader.read_i32();
			const int32_t bottom = reader.read_i32();
			glyph.subrect = RectangleI(left, top, right - left, bottom - top);

			glyph.x_offset = reader.read_f32();
			glyph.y_offset = reader.read_f32();
			glyph.x_advance = reader.read_f32();

			font.glyphs.push_back(glyph);
		}

		font.line_spacing = reader.read_f32();
		font.stand_in = static_cast<char32_t>(reader.read_u32());

		const int width = static_cast<int>(reader.read_u32());
		const int height = static_cast<int>(reader.read_u32());
		const TextureFormat format = to_texture_format(reader.read_u32(), path);
		const int stride = static_cast<int>(reader.read_u32());
		const int rows = static_cast<int>(reader.read_u32());

		if (width <= 0 || height <= 0 || stride <= 0 || rows <= 0)
		{
			throw std::runtime_error(path + " describes an atlas of " +
				std::to_string(width) + "x" + std::to_string(height) +
				" in " + std::to_string(rows) + " rows of " +
				std::to_string(stride) + " bytes, which is not an atlas.");
		}

		// THE FILE'S OWN STRIDE AND ROW COUNT, NOT THE COMPUTED ONES. A
		// .spritefont states both, and it is the only file this engine reads
		// that does - so they are taken as written and then checked against
		// what the format says they should be. A disagreement is a file this
		// engine would read the wrong number of bytes from, which is worth a
		// message rather than a texture full of noise.
		const TextureLevel expected = texture_level(format, width, height, 0);
		if (stride != expected.stride || rows != expected.rows)
		{
			throw std::runtime_error(path + " says its atlas is " +
				std::to_string(rows) + " rows of " + std::to_string(stride) +
				" bytes, but a " + std::to_string(width) + "x" +
				std::to_string(height) + " image in that format is " +
				std::to_string(expected.rows) + " rows of " +
				std::to_string(expected.stride) + ".");
		}

		font.atlas.width = width;
		font.atlas.height = height;
		font.atlas.format = format;
		font.atlas.levels.push_back(expected);
		font.atlas.pixels = reader.read_bytes(expected.size);

		// Anything after the atlas is left alone rather than rejected: a future
		// MakeSpriteFont appending a field is the likeliest way a file gets
		// longer, and what matters is that everything read above was there.
		return font;
	}
}
