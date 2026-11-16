#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace labrador::detail
{
	// The browser bridge passes counts as signed i32. A zero-length region
	// means the whole wave, so its unused start is not narrowed or checked.
	inline void check_web_audio_wave_counts(std::size_t frames, uint32_t loop_start,
		uint32_t loop_length, const std::string& bank, const std::string& wave)
	{
		const std::size_t limit = static_cast<std::size_t>(std::numeric_limits<int>::max());
		if (frames > limit || loop_length > limit || (loop_length != 0 && loop_start > limit))
		{
			throw std::runtime_error("Failed to open wave bank '" + bank +
				"' wave '" + wave + "': frame or loop count exceeds the Web Audio bridge range.");
		}
	}
}
