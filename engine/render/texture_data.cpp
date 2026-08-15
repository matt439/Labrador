#include "engine/render/texture_data.h"

namespace labrador
{
	namespace
	{
		// Bytes per 4x4 block for a compressed format, or bytes per pixel for
		// one that is not. The two are never confused because nothing outside
		// this file reads either.
		int unit_bytes(TextureFormat format)
		{
			switch (format)
			{
			case TextureFormat::bc1_unorm:      return 8;
			case TextureFormat::bc2_unorm:      return 16;
			case TextureFormat::bc3_unorm:      return 16;
			case TextureFormat::b4g4r4a4_unorm: return 2;
			case TextureFormat::r8g8b8a8_unorm:
			case TextureFormat::b8g8r8a8_unorm:
			default:                            return 4;
			}
		}
	}

	bool is_block_compressed(TextureFormat format)
	{
		return format == TextureFormat::bc1_unorm ||
			format == TextureFormat::bc2_unorm ||
			format == TextureFormat::bc3_unorm;
	}

	TextureLevel texture_level(TextureFormat format, int width, int height,
		size_t offset)
	{
		TextureLevel level;
		level.width = width;
		level.height = height;
		level.offset = offset;

		if (is_block_compressed(format))
		{
			// A ROUND UP AND A FLOOR OF ONE, both of which matter at the bottom
			// of a mip chain. A 6x6 level is two blocks by two, not one and a
			// half, and a 1x1 level is still a whole block - so a chain that
			// walked down to nothing would stop one level early and leave the
			// last level's bytes unread, shifting every level after it.
			const int blocks_wide = (width + 3) / 4;
			const int blocks_high = (height + 3) / 4;
			level.stride = (blocks_wide < 1 ? 1 : blocks_wide) *
				unit_bytes(format);
			level.rows = blocks_high < 1 ? 1 : blocks_high;
		}
		else
		{
			level.stride = width * unit_bytes(format);
			level.rows = height;
		}

		level.size = static_cast<size_t>(level.stride) *
			static_cast<size_t>(level.rows);
		return level;
	}
}
