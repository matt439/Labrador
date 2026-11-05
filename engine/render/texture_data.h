#pragma once

#include "engine/render/texture_format.h"

#include <cstddef>
#include <string>
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
	// being small. A backend whose device cannot sample the format either says
	// so (T6) or asks for the pixels itself, through block_decode.h; it is
	// never handed a decompressed copy behind its back, so a device that takes
	// the blocks gets the blocks.
	struct TextureData
	{
		int width = 0;
		int height = 0;
		TextureFormat format = TextureFormat::r8g8b8a8_unorm;

		// At least one, and level zero is the full-size image. More only when
		// the file carried a mip chain.
		//
		// A CHAIN IS READ AND UPLOADED AND NEVER SAMPLED FROM. A minified draw
		// samples level zero, which renderer.h states beside set_filter. The
		// levels stay here and still reach the device: refusing them would make
		// this struct a worse description of a .dds without making anything
		// safer.
		std::vector<TextureLevel> levels;

		std::vector<unsigned char> pixels;
	};

	// The widest or tallest image a reader turns into a texture.
	//
	// 16384 is the two-dimensional texture limit Direct3D 11 guarantees at
	// feature level 11_0 and above, so a larger picture could not become a
	// texture on such a device anyway; a feature level 10 device stops at
	// 8192. The PNG reader refuses a header beyond it before
	// decoding, so a few bytes of compressed file cannot ask for gigabytes; a
	// .dds needs no such check, because its pixels must all be in the file.
	// A sprite sheet's animation strip, whose frames sit side by side in one
	// atlas, may have at most this many.
	inline constexpr int max_texture_dimension = 16384;

	// Whether `format` stores 4x4 blocks rather than pixels.
	bool is_block_compressed(TextureFormat format);

	// The layout of one level of `format` at this size, starting at `offset`.
	//
	// This is the arithmetic a file format either states or expects the reader
	// to know, and the block case is the one a reader that guesses gets wrong.
	// It is in one place so that the .dds reader, which computes it for every
	// mip, and the .spritefont reader, which checks an atlas against it, agree
	// by construction.
	// Throws std::runtime_error naming context for nonpositive dimensions,
	// a row wider than int can hold, or an unrepresentable size/end offset.
	TextureLevel texture_level(TextureFormat format, int width, int height,
		size_t offset, const std::string& context = "texture_level");
}
