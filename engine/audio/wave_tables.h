#pragma once

#include "engine/audio/audio_device.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace labrador
{
	// One voice's state as the seam reports it, transcribed from DirectXTK's
	// SoundCommon.h rather than invented, so that a backend without DirectXTK
	// reports the transitions the one with it does.
	struct VoiceRecord
	{
		AudioDevice::WaveBankHandle bank;
		int wave = -1;
		SoundState state = SoundState::stopped;
		bool looping = false;
	};

	// The wave-name and voice tables the null and Web Audio backends keep
	// for themselves, where xaudio2/ asks DirectXTK. Every bounds check
	// throws the seam's std::out_of_range in the words xaudio2/ uses for
	// the same mistake, so that no client can tell the backends apart by
	// how they refuse it.
	//
	// Not a public type: AudioDevice is the game's way in. A header alone,
	// so that each backend stays the one translation unit CMake selects.
	class WaveTables
	{
	public:
		// Keeps a bank's wave names, in the order whose index wave_index
		// answers with.
		AudioDevice::WaveBankHandle add_bank(std::vector<std::string> names)
		{
			this->banks_.push_back(std::move(names));
			return AudioDevice::WaveBankHandle(
				static_cast<int>(this->banks_.size()) - 1);
		}

		// The index of `wave_name` in the bank, or -1 for a name the bank
		// does not have, which is the seam's spelling and DirectXTK's.
		int wave_index(AudioDevice::WaveBankHandle bank,
			const std::string& wave_name) const
		{
			const std::vector<std::string>& names = this->bank(bank);
			const std::vector<std::string>::const_iterator found =
				std::find(names.begin(), names.end(), wave_name);
			return found == names.end()
				? -1
				: static_cast<int>(found - names.begin());
		}

		// Refuses a wave index the bank does not hold, for create_voice.
		void check_voice_wave(AudioDevice::WaveBankHandle bank,
			int wave_index) const
		{
			if (!this->holds(bank, wave_index))
			{
				throw std::out_of_range(
					"A voice was asked for a wave the bank does not hold.");
			}
		}

		// Refuses a wave index the bank does not hold, for play_wave.
		void check_played_wave(AudioDevice::WaveBankHandle bank,
			int wave_index) const
		{
			if (!this->holds(bank, wave_index))
			{
				throw std::out_of_range(
					"A wave was played that the bank does not hold.");
			}
		}

		// Keeps a voice's state and returns the index it was kept at.
		int add_voice(const VoiceRecord& voice)
		{
			this->voices_.push_back(voice);
			return static_cast<int>(this->voices_.size()) - 1;
		}

		VoiceRecord& voice(AudioDevice::VoiceHandle handle)
		{
			const size_t index = static_cast<size_t>(handle.index());
			if (!handle.valid() || index >= this->voices_.size())
			{
				throw std::out_of_range(
					"A voice was reached through an unresolved handle.");
			}
			return this->voices_[index];
		}

		// A bank's wave names, as add_bank kept them.
		const std::vector<std::string>& bank(
			AudioDevice::WaveBankHandle handle) const
		{
			const size_t index = static_cast<size_t>(handle.index());
			if (!handle.valid() || index >= this->banks_.size())
			{
				throw std::out_of_range(
					"A wave bank was reached through an unresolved handle.");
			}
			return this->banks_[index];
		}

	private:
		std::vector<std::vector<std::string>> banks_;
		std::vector<VoiceRecord> voices_;

		bool holds(AudioDevice::WaveBankHandle bank, int wave_index) const
		{
			const std::vector<std::string>& names = this->bank(bank);
			return wave_index >= 0 &&
				static_cast<size_t>(wave_index) < names.size();
		}
	};
}
