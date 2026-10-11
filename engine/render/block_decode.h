#pragma once

#include "engine/render/texture_data.h"

namespace labrador
{
	// A block-compressed texture as the r8g8b8a8 pixels it encodes, every level.
	//
	// FOR A DEVICE THAT CANNOT SAMPLE BLOCK COMPRESSION, and for nothing else.
	// texture_data.h says why the engine hands a backend the blocks rather than
	// the pixels, and that stays the rule: a device that takes BC1-BC3 gets
	// them as they are. This is the other answer, for the device that does not
	// - a WebGL2 context without WEBGL_compressed_texture_s3tc, which is most
	// phones - and the backend that holds one calls it in place of refusing
	// the texture. The price is the memory compression saved: four times the
	// bytes for BC2 and BC3 and eight for BC1, on the device least likely to
	// have them, which is why it is a fallback and not a default.
	//
	// ENGINE CODE, BECAUSE A DECODER DECIDES WHAT A PIXEL IS. The two colours a
	// third and two thirds of the way between a block's endpoints, BC1's
	// midpoint, and BC3's six or four interpolated alphas are rounded somewhere,
	// and no specification fixes where; device decoders disagree by a few
	// levels. Here every one of them is rounded to nearest, an endpoint is
	// widened from 5:6:5 by repeating its high bits, and a BC2 alpha is its
	// four bits times 17 - written once, tested headlessly, and the same on
	// every device that falls back to it.
	//
	// WHAT EACH FORMAT DECODES TO, which is the layout the blocks define and not
	// a choice made here. BC1 has two modes, chosen per block by its endpoints:
	// the first endpoint above the second gives four colours, otherwise three
	// and a transparent black, whose four channels are all zero - which is what
	// transparent is under this engine's premultiplied blend. BC2 and BC3 carry
	// alpha separately and their colour block is always the four-colour mode,
	// whichever endpoint is larger. BC2's alpha is explicit, four bits a texel.
	// BC3's is two endpoints and three-bit indices: the first above the second
	// gives six interpolated alphas, otherwise four and the two extremes 0 and
	// 255. Nothing is premultiplied or unpremultiplied on the way through.
	//
	// THE RESULT HAS THE SAME LEVELS, at the same sizes, packed one after
	// another; a level whose size is not a multiple of four keeps only the
	// texels inside it, so a 5x3 level comes back as fifteen pixels.
	//
	// Throws std::runtime_error if `texture` is not one of the three
	// block-compressed formats, has no levels, or has a level whose layout is
	// not the one texture_level gives for its size or whose bytes run past the
	// end of its pixels.
	TextureData decode_block_compressed(const TextureData& texture);
}
