#include "engine/render/texture_data.h"

#include <limits>
#include <stdexcept>
#include <string>

namespace labrador
{
	namespace
	{
		// Bytes per 4x4 block for a compressed format, or bytes per pixel for
		// one that is not. The two are never confused because nothing outside
		// this file reads either.
		//
		// No default label, so a new enumerator is a -Wswitch error under
		// clang rather than a silent four bytes. A value outside the enum is
		// refused by name.
		int unit_bytes(TextureFormat format, const std::string& context)
		{
			switch (format)
			{
			case TextureFormat::bc1_unorm:      return 8;
			case TextureFormat::bc2_unorm:      return 16;
			case TextureFormat::bc3_unorm:      return 16;
			case TextureFormat::b4g4r4a4_unorm: return 2;
			case TextureFormat::r8g8b8a8_unorm: return 4;
			case TextureFormat::b8g8r8a8_unorm: return 4;
			}
			throw std::runtime_error(context + ": TextureFormat " +
				std::to_string(static_cast<int>(format)) +
				" is not a format this engine lays out.");
		}
	}

	bool is_block_compressed(TextureFormat format)
	{
		switch (format)
		{
		case TextureFormat::bc1_unorm:
		case TextureFormat::bc2_unorm:
		case TextureFormat::bc3_unorm:
			return true;
		case TextureFormat::r8g8b8a8_unorm:
		case TextureFormat::b8g8r8a8_unorm:
		case TextureFormat::b4g4r4a4_unorm:
			return false;
		}
		return false;
	}

	TextureLevel texture_level(TextureFormat format, int width, int height,
		size_t offset, const std::string& context)
	{
		if (width <= 0 || height <= 0)
		{
			throw std::runtime_error(context + ": texture dimensions must be positive.");
		}
		TextureLevel level;
		level.width = width;
		level.height = height;
		level.offset = offset;

		size_t columns = static_cast<size_t>(width);
		size_t rows = static_cast<size_t>(height);
		if (is_block_compressed(format))
		{
			// A ROUND UP AND A FLOOR OF ONE, both of which matter at the bottom
			// of a mip chain. A 6x6 level is two blocks by two, not one and a
			// half, and a 1x1 level is still a whole block - so a chain that
			// walked down to nothing would stop one level early and leave the
			// last level's bytes unread, shifting every level after it.
			columns = (columns + 3) / 4;
			rows = (rows + 3) / 4;
		}
		const size_t bytes = static_cast<size_t>(unit_bytes(format, context));
		if (columns > static_cast<size_t>((std::numeric_limits<int>::max)()) / bytes)
		{
			throw std::runtime_error(context + ": texture row stride exceeds int range.");
		}
		const size_t stride = columns * bytes;
		if (rows > ((std::numeric_limits<size_t>::max)() - offset) / stride)
		{
			throw std::runtime_error(context + ": texture size or accumulated offset overflow.");
		}
		level.stride = static_cast<int>(stride);
		level.rows = static_cast<int>(rows);
		level.size = stride * rows;
		return level;
	}
}
