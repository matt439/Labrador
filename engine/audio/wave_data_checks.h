#pragma once

#include "engine/audio/wave_data.h"

#include <limits>

namespace labrador::detail
{
	// The portable route must fit XAudio2's submitted-source-buffer maximum.
	inline constexpr size_t max_pcm_bytes = 0x80000000u;

	constexpr bool portable_pcm_size_fits(size_t samples, size_t frames) noexcept
	{
		return samples <= max_pcm_bytes / sizeof(int16_t) &&
			frames <= static_cast<size_t>(std::numeric_limits<int>::max());
	}

	// Load-time checks shared by the memory-bank backends. This does not
	// publish data, mutate the bank or depend on an audio API.
	// Throws std::invalid_argument with bank/wave context; bad_alloc propagates.
	void validate_wave_bank_data(const WaveBankData& bank);
}
