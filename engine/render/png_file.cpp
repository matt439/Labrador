#include "engine/render/png_file.h"

#include "engine/core/byte_reader.h"
#include "engine/render/texture_format.h"

#include <lodepng.h>

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace labrador
{
	TextureData read_png_file(const std::string& path)
	{
		return decode_png(read_file_bytes(path), path);
	}

	TextureData decode_png(const std::vector<unsigned char>& bytes,
		const std::string& context)
	{
		std::vector<unsigned char> rgba;
		unsigned width = 0;
		unsigned height = 0;
		const unsigned failed =
			lodepng::decode(rgba, width, height, bytes, LCT_RGBA, 8);
		if (failed != 0)
		{
			throw std::runtime_error(context + ": not a PNG this engine can read: " +
				lodepng_error_text(failed));
		}

		const unsigned largest =
			static_cast<unsigned>(std::numeric_limits<int>::max());
		if (width == 0 || height == 0 || width > largest || height > largest)
		{
			throw std::runtime_error(context + ": a PNG of " +
				std::to_string(width) + " by " + std::to_string(height) +
				" pixels has no texture size");
		}

		// Premultiplied, rounded to nearest: (c * a + 127) / 255 is exact for
		// a = 255 and a = 0, and never above a, so an opaque pixel keeps its
		// colour and a clear one adds nothing when drawn.
		for (size_t pixel = 0; pixel + 3 < rgba.size(); pixel += 4)
		{
			const unsigned alpha = rgba[pixel + 3];
			for (size_t channel = 0; channel < 3; ++channel)
			{
				const unsigned value = rgba[pixel + channel];
				rgba[pixel + channel] =
					static_cast<unsigned char>((value * alpha + 127u) / 255u);
			}
		}

		TextureData texture;
		texture.width = static_cast<int>(width);
		texture.height = static_cast<int>(height);
		texture.format = TextureFormat::r8g8b8a8_unorm;
		texture.levels.push_back(texture_level(texture.format, texture.width,
			texture.height, 0, context));
		if (texture.levels.front().size != rgba.size())
		{
			throw std::runtime_error(context +
				": the decoded PNG is not the size its header gives");
		}
		texture.pixels = std::move(rgba);
		return texture;
	}
}
