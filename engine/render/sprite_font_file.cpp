#include "engine/render/sprite_font_file.h"

#include "engine/math/rectanglei.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace mattmath;

namespace artattack
{
	namespace
	{
		const char MAGIC[] = "DXTKfont";
		const size_t MAGIC_LENGTH = 8;

		// The format numbers are DXGI's, and this is the only place in the
		// engine allowed to know one - because they are not the backend's
		// choice here, they are what is written in the file. A file format that
		// happens to be spelt in another API's constants is still a file
		// format.
		const uint32_t DXGI_R8G8B8A8_UNORM = 28;
		const uint32_t DXGI_BC2_UNORM = 74;
		const uint32_t DXGI_B4G4R4A4_UNORM = 115;

		// A cursor over the file's bytes that cannot walk off the end.
		//
		// EVERY READ IS CHECKED, which the obvious alternative - map a packed
		// struct over the buffer - is not. A truncated font is a plausible
		// thing to have on disk (an interrupted copy, a half-written build
		// step), and the difference between the two designs is a named throw
		// and a read past the end of a heap allocation.
		class Reader
		{
		public:
			Reader(const std::vector<unsigned char>& bytes,
				const std::string& path)
				: bytes_(bytes), path_(path)
			{

			}

			uint32_t read_u32()
			{
				this->require(4);
				const size_t at = this->at_;
				this->at_ += 4;
				return static_cast<uint32_t>(this->bytes_[at]) |
					(static_cast<uint32_t>(this->bytes_[at + 1]) << 8) |
					(static_cast<uint32_t>(this->bytes_[at + 2]) << 16) |
					(static_cast<uint32_t>(this->bytes_[at + 3]) << 24);
			}

			int32_t read_i32()
			{
				return static_cast<int32_t>(this->read_u32());
			}

			// Through the bit pattern, because that is what the file holds: a
			// little-endian IEEE-754 single. Reading it as an integer and
			// copying the bits is the only spelling that does not depend on
			// this compiler's struct layout.
			float read_f32()
			{
				const uint32_t bits = this->read_u32();
				float value = 0.0f;
				static_assert(sizeof(value) == sizeof(bits),
					"A .spritefont stores 32-bit floats.");
				std::memcpy(&value, &bits, sizeof(value));
				return value;
			}

			std::vector<unsigned char> read_bytes(size_t count)
			{
				this->require(count);
				const std::vector<unsigned char>::const_iterator first =
					this->bytes_.begin() +
					static_cast<std::ptrdiff_t>(this->at_);
				this->at_ += count;
				return std::vector<unsigned char>(first,
					first + static_cast<std::ptrdiff_t>(count));
			}

			void skip(size_t count)
			{
				this->require(count);
				this->at_ += count;
			}

			void require(size_t count) const
			{
				if (this->at_ + count > this->bytes_.size())
				{
					throw std::runtime_error(path_ +
						" is not a whole .spritefont: it ends after " +
						std::to_string(this->bytes_.size()) +
						" bytes, part way through a record.");
				}
			}

		private:
			const std::vector<unsigned char>& bytes_;
			const std::string& path_;
			size_t at_ = 0;
		};

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

		std::vector<unsigned char> read_whole_file(const std::string& path)
		{
			std::ifstream file(path, std::ios::binary | std::ios::ate);
			if (!file)
			{
				throw std::out_of_range("SpriteFont not found at " + path +
					".");
			}

			const std::streamoff size = file.tellg();
			if (size < 0)
			{
				throw std::runtime_error("Could not measure " + path + ".");
			}

			std::vector<unsigned char> bytes(static_cast<size_t>(size));
			file.seekg(0, std::ios::beg);
			if (!bytes.empty())
			{
				file.read(reinterpret_cast<char*>(bytes.data()), size);
			}
			if (!file)
			{
				throw std::runtime_error("Could not read " + path + ".");
			}
			return bytes;
		}
	}

	SpriteFontFile read_sprite_font_file(const std::string& path)
	{
		const std::vector<unsigned char> bytes = read_whole_file(path);
		Reader reader(bytes, path);

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
		reader.require(static_cast<size_t>(glyph_count) * 32u);
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

		font.width = static_cast<int>(reader.read_u32());
		font.height = static_cast<int>(reader.read_u32());
		font.format = to_texture_format(reader.read_u32(), path);
		font.stride = static_cast<int>(reader.read_u32());
		font.rows = static_cast<int>(reader.read_u32());

		if (font.width <= 0 || font.height <= 0 || font.stride <= 0 ||
			font.rows <= 0)
		{
			throw std::runtime_error(path + " describes an atlas of " +
				std::to_string(font.width) + "x" +
				std::to_string(font.height) + " in " +
				std::to_string(font.stride) + " byte rows, which is not an "
				"atlas.");
		}

		font.pixels = reader.read_bytes(static_cast<size_t>(font.stride) *
			static_cast<size_t>(font.rows));

		// Anything after the atlas is left alone rather than rejected: a future
		// MakeSpriteFont appending a field is the likeliest way a file gets
		// longer, and what matters is that everything read above was there.
		return font;
	}
}
