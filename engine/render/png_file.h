#pragma once

#include "engine/render/texture_data.h"

#include <string>
#include <vector>

namespace labrador
{
	// Reads the .png at `path` as one level of r8g8b8a8_unorm, premultiplied.
	//
	// A PNG is the picture a paint program saves, so a game can draw one
	// with no converter in between. Whatever the file stores - palette, grey,
	// 16 bits a channel, with or without alpha - comes out as 8-bit RGBA, and
	// each colour channel is multiplied by its alpha, rounded to the nearest
	// whole value, because every draw blends premultiplied (renderer.h). A
	// file's gamma and colour-profile chunks are not applied.
	//
	// WHAT IT COSTS AGAINST A .dds. Four bytes a pixel on the device, where
	// bc3_unorm takes one, no mip chain, and the decode at every load and
	// device restore. A texture that is large or many stays a block-compressed
	// .dds, which load_texture_asset reads by the same name.
	//
	// Throws std::out_of_range naming the path if there is no file there, and
	// std::runtime_error naming the path and the decoder's reason if there is
	// one and it is not a PNG it can decode (T6). A header giving either side
	// above max_texture_dimension (texture_data.h) is refused the same way,
	// before any pixel is decoded.
	TextureData read_png_file(const std::string& path);

	// The same for PNG bytes already in memory; `context` names them in a
	// throw, as the path does above.
	TextureData decode_png(const std::vector<unsigned char>& bytes,
		const std::string& context);
}
