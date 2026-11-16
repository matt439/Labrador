#include "engine/audio/wave_data.h"
#include "engine/audio/wave_data_checks.h"

#include <limits>
#include <set>
#include <stdexcept>
#include <string_view>

namespace labrador::detail
{
	namespace
	{
		bool valid_name(const std::string& name)
		{
			return !name.empty() && name.find('\0') == std::string::npos;
		}
	}

	void validate_wave_bank_data(const WaveBankData& bank)
	{
		if (!valid_name(bank.name))
		{
			throw std::invalid_argument("Memory wave bank needs a nonempty name without embedded NUL");
		}
		const std::string label = "Memory wave bank '" + bank.name + "'";
		if (bank.waves.empty() || bank.waves.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
		{
			throw std::invalid_argument(label + ": wave count must be nonzero and fit an int index");
		}

		std::set<std::string_view> names;
		for (size_t index = 0; index < bank.waves.size(); ++index)
		{
			const WaveData& wave = bank.waves[index];
			const std::string position = label + ": wave " + std::to_string(index);
			if (!valid_name(wave.name))
			{
				throw std::invalid_argument(position + " needs a nonempty name without embedded NUL");
			}
			const std::string wave_label = position + " ('" + wave.name + "')";
			if (!names.insert(wave.name).second)
			{
				throw std::invalid_argument(wave_label + ": duplicate wave name");
			}
			if (wave.encoding != WaveEncoding::pcm8 && wave.encoding != WaveEncoding::pcm16 &&
				wave.encoding != WaveEncoding::adpcm)
			{
				throw std::invalid_argument(wave_label + ": unsupported source encoding");
			}
			if (wave.channels != 1 && wave.channels != 2)
			{
				throw std::invalid_argument(wave_label + ": only mono and stereo PCM are supported");
			}
			if (wave.sample_rate < 8000 || wave.sample_rate > 96000)
			{
				throw std::invalid_argument(wave_label + ": sample rate must be 8000..96000 Hz");
			}
			if (wave.samples.empty() || wave.samples.size() % static_cast<size_t>(wave.channels) != 0)
			{
				throw std::invalid_argument(wave_label + ": samples must contain nonempty whole interleaved frames");
			}
			if (!portable_pcm_size_fits(wave.samples.size(), wave.frames()))
			{
				throw std::invalid_argument(wave_label + ": decoded PCM exceeds the audio API size limits");
			}
			if (wave.loop_length != 0 &&
				(static_cast<size_t>(wave.loop_start) >= wave.frames() ||
					static_cast<size_t>(wave.loop_length) > wave.frames() - static_cast<size_t>(wave.loop_start)))
			{
				throw std::invalid_argument(wave_label + ": loop region is outside the wave");
			}
		}
	}
}
