#pragma once

#include "engine/audio/wave_data.h"

#include <string>

namespace labrador
{
	// Reads the XACT wave bank (.xwb) at `path`.
	//
	// THE ENGINE READS IT, NOT A BACKEND, for the reason dds_file.h gives for
	// textures: a reader inside a library that exists for one audio API is the
	// only description of what this engine's sound content is, and every other
	// backend would have to find another reader and hope it agreed. This one
	// is where the format is written down, once, for any backend whose API has
	// no reader of its own. The seam still takes a bank name rather than a
	// file (audio_device.h), so a backend that has a reader may keep it.
	//
	// EVERYTHING COMES OUT AS SIGNED 16-BIT PCM, WHICH IS WHERE THIS DIFFERS
	// FROM A TEXTURE. TextureData keeps a compressed format compressed because
	// a GPU samples it that way. No audio API this engine is likely to sit on
	// outside Windows plays MS-ADPCM or 8-bit unsigned PCM from a buffer - Web
	// Audio and AAudio take 16-bit or float - so every caller of this reader
	// would decode at once, and decoding here makes that one decoder rather
	// than one per backend. The cost is memory: an ADPCM wave occupies about
	// four times its file size once read, which is stated rather than hidden.
	//
	// WHAT IT READS: little-endian banks of version 44 with standard (not
	// compact) entries, in-memory or streaming, holding PCM at 8 or 16 bits or
	// MS-ADPCM with the standard seven coefficient pairs - which is what
	// `xwbtool -nc` writes. Names are read when the bank carries a name table.
	//
	// WHAT IT REFUSES, EACH BY NAME (T6), and what to do about it: xWMA and XMA
	// entries, which are codecs this engine does not decode; compact entries,
	// which `xwbtool -nc` does not write; a big-endian (Xbox 360) bank; and an
	// ADPCM entry that does not end on a whole block, which no XAudio2 voice
	// can play either. Seek tables exist only for xWMA and are not read.
	//
	// Throws std::out_of_range naming the path if there is no file there, and
	// std::runtime_error naming the path AND what is wrong with it otherwise,
	// including the index and name of the entry when one entry is at fault.
	WaveBankData read_xwb_file(const std::string& path);
}
