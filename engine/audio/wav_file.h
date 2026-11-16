#pragma once

#include "engine/audio/wave_data.h"

#include <string>

namespace labrador
{
	// Reads little-endian RIFF/WAVE PCM: unsigned 8-bit or signed 16-bit,
	// mono/stereo, 8000..96000 Hz, with a 16-byte fmt chunk or an 18-byte one
	// whose extension size is zero. Samples become owned interleaved signed
	// 16-bit PCM; unsigned 8-bit conversion is (value - 128) * 256. name is
	// empty because a WAV carries no bank-resolution name; the caller sets it.
	//
	// Unknown chunks and their WORD padding are skipped. RIFF/chunk sizes,
	// byte rate, block alignment and complete frames are checked. Duplicate
	// fmt/data/smpl chunks, empty data, other encodings and extensible WAV are
	// refused. Optional smpl holds at most one forward, integral, infinite
	// loop with an inclusive end frame; the result stores start and length.
	// Without a loop, both loop fields are zero. Other sampler metadata and
	// sampler-specific data are ignored.
	// Decoded PCM is limited to 2 GiB per wave and frame counts fit int.
	//
	// The file and decoded samples coexist during loading; PCM8 expands to
	// twice its payload bytes. No compressed codec or streaming is provided.
	// Throws std::out_of_range for a missing path, std::invalid_argument for
	// an embedded path NUL, and std::runtime_error naming path and fault for
	// unreadable or malformed content. std::bad_alloc propagates unchanged.
	WaveData read_wav_file(const std::string& path);
}
