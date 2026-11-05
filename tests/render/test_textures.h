#pragma once

#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"

#include <cstddef>

namespace render_tests
{
	// Two by two, one opaque colour, one level of r8g8b8a8_unorm. For the
	// cases that need a texture to exist, or that care which bytes a name is
	// holding, rather than what an atlas looks like. Inline so that every test
	// executable and opt-in probe can use it without another source to list.
	inline labrador::TextureData flat_texture(unsigned char red,
		unsigned char green, unsigned char blue)
	{
		labrador::TextureData texture;
		texture.width = 2;
		texture.height = 2;
		texture.format = labrador::TextureFormat::r8g8b8a8_unorm;
		texture.levels.push_back(labrador::texture_level(texture.format, 2, 2, 0));
		texture.pixels.resize(texture.levels[0].size);

		for (std::size_t byte = 0; byte < texture.pixels.size(); byte += 4)
		{
			texture.pixels[byte] = red;
			texture.pixels[byte + 1] = green;
			texture.pixels[byte + 2] = blue;
			texture.pixels[byte + 3] = 255;
		}

		return texture;
	}
}
