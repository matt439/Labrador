#include "engine/render/texture_format.h"

namespace labrador
{
	std::string format_name(TextureFormat format)
	{
		switch (format)
		{
		case TextureFormat::r8g8b8a8_unorm: return "r8g8b8a8_unorm";
		case TextureFormat::b8g8r8a8_unorm: return "b8g8r8a8_unorm";
		case TextureFormat::b4g4r4a4_unorm: return "b4g4r4a4_unorm";
		case TextureFormat::bc1_unorm:      return "bc1_unorm";
		case TextureFormat::bc2_unorm:      return "bc2_unorm";
		case TextureFormat::bc3_unorm:      return "bc3_unorm";
		}
		return "TextureFormat " + std::to_string(static_cast<int>(format));
	}
}
