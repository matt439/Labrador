#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace labrador
{
	// The source encoding before decoding. Samples below are signed 16-bit
	// PCM whatever this says; the value preserves their provenance.
	enum class WaveEncoding
	{
		pcm8,
		pcm16,
		adpcm
	};

	// One owned wave, decoded to signed 16-bit PCM.
	struct WaveData
	{
		// A name used to resolve this wave in a memory bank. A reader may
		// leave it empty when the source format carries no name.
		std::string name;

		WaveEncoding encoding = WaveEncoding::pcm16;
		int channels = 0;
		int sample_rate = 0;

		// Frames, not individual interleaved samples. A zero length means no
		// region: loop_start is ignored and a looping voice loops the whole
		// wave. Otherwise it plays from the start, then repeats this region.
		uint32_t loop_start = 0;
		uint32_t loop_length = 0;

		// Interleaved: a frame is `channels` consecutive samples.
		std::vector<int16_t> samples;

		size_t frames() const
		{
			return this->channels > 0
				? this->samples.size() / static_cast<size_t>(this->channels)
				: 0;
		}
	};

	// An owned bank and its waves in resolution order.
	struct WaveBankData
	{
		std::string name;
		std::vector<WaveData> waves;
	};

}
