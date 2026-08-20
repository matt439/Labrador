#pragma once

#include "engine/render/texture_format.h"

#include <cstddef>
#include <vector>

namespace labrador
{
	// One mip level's place in a texture's bytes.
	//
	// STRIDE IS PER ROW OF THE LAYOUT, NOT PER ROW OF PIXELS, and that is the
	// distinction the whole struct exists to keep. For a block-compressed
	// format a "row" is a row of 4x4 blocks, so a 128-pixel-wide BC2 level has
	// a stride of 512 bytes and 36 rows for its 144 pixels of height. Deriving
	// either from width and height without knowing the format gives an answer
	// four times too large, which a device reports as a corrupt texture or not
	// at all.
	struct TextureLevel
	{
		int width = 0;
		int height = 0;
		int stride = 0;
		int rows = 0;

		// Into TextureData::pixels.
		size_t offset = 0;
		size_t size = 0;
	};

	// A texture, decoded far enough to hand to a device and no further.
	//
	// THE HANDOVER POINT BETWEEN THE ENGINE AND A BACKEND, and it is deliberately
	// low: the engine says what the bytes are and where each level starts, and
	// the backend does nothing but pass that to whatever its API calls a texture.
	// Both of the two things this engine loads - a .dds and the atlas inside a
	// .spritefont - come out as one of these, so a backend has one texture
	// creation function rather than one per file kind.
	//
	// NOT DECODED FURTHER THAN THIS, on purpose. A BC3 blob stays a BC3 blob:
	// unpacking it here would throw away the whole reason the content is
	// compressed, which is the memory it does not occupy on a machine chosen for
	// being small. A backend that cannot upload the format says so (T6); it does
	// not get handed a decompressed copy behind its back.
	struct TextureData
	{
		int width = 0;
		int height = 0;
		TextureFormat format = TextureFormat::r8g8b8a8_unorm;

		// At least one, and level zero is the full-size image. More only when
		// the file carried a mip chain, which no file in either client does -
		// all forty-three .dds between them are single-level.
		//
		// A CHAIN IS READ AND UPLOADED AND NEVER SAMPLED FROM. A minified draw
		// samples level zero, which renderer.h states beside set_filter and
		// which the rasterising backends answered differently until it was
		// stated - one walked the chain, the others did not, and they agreed
		// only because there has never been a chain to disagree about. The levels
		// stay here and still reach the device: refusing them would make this
		// struct a worse description of a .dds without making anything safer.
		std::vector<TextureLevel> levels;

		std::vector<unsigned char> pixels;
	};

	// Whether `format` stores 4x4 blocks rather than pixels.
	bool is_block_compressed(TextureFormat format);

	// The layout of one level of `format` at this size, starting at `offset`.
	//
	// This is the arithmetic a file format either states or expects the reader
	// to know, and every reader that guessed it got the block case wrong. It is
	// in one place so that the .dds reader, which must compute it for every mip,
	// and any later reader agree by construction.
	TextureLevel texture_level(TextureFormat format, int width, int height,
		size_t offset);
}
