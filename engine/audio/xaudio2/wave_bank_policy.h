#pragma once

#include "engine/audio/xaudio2/device_recovery.h"

#include <memory>
#include <new>
#include <stdexcept>
#include <string>

// What this backend decides around DirectXTK's WaveBank, written against the
// few members it calls so tests/audio/xaudio2_recovery_tests.cpp can check it
// with a stand-in bank and no audio graph. `Bank` is DirectX::WaveBank and
// `Format` is WAVEFORMATEX in audio_device.cpp.
namespace labrador::detail
{
	// Whether `wave_index` names an entry of `bank`. WaveBank has no entry
	// count: Play drops an index past the end without a word, and
	// CreateInstance answers null for one. Its size and format queries are
	// the two that answer nothing only past the end, so an entry is held if
	// either answers. The 64-byte format buffer is the one DirectXTK's own
	// Play reads a format into.
	template<typename Format, typename Bank>
	bool holds_wave(const Bank& bank, int wave_index)
	{
		if (wave_index < 0)
		{
			return false;
		}

		const unsigned int index = static_cast<unsigned int>(wave_index);
		if (bank.GetSampleSizeInBytes(index) != 0)
		{
			return true;
		}

		alignas(Format) unsigned char format[64] = {};
		return bank.GetFormat(index, reinterpret_cast<Format*>(format),
			sizeof(format)) != nullptr;
	}

	// Opens the bank at `path` through `open`, which constructs it.
	//
	// Any failure to read the container becomes the std::runtime_error naming
	// the file that audio_device.h promises, keeping the reader's own words:
	// DirectXTK's what() alone is "WaveBank". std::bad_alloc is not a missing
	// or unreadable container, so it propagates unchanged rather than being
	// handled as an optional bank that is absent.
	//
	// A streaming bank is refused here, naming the file. DirectXTK reads one
	// and then refuses every one-shot and every effect instance it is asked
	// for, which would surface during play rather than at load.
	template<typename Bank, typename Open>
	std::unique_ptr<Bank> open_wave_bank(const std::string& path, Open&& open)
	{
		std::unique_ptr<Bank> bank;
		try
		{
			bank = open();
		}
		catch (const std::bad_alloc&)
		{
			throw;
		}
		catch (const std::exception& unread)
		{
			throw std::runtime_error("Failed to open wave bank: " + path +
				" (" + unread.what() + ")");
		}

		if (bank->IsStreamingBank())
		{
			throw std::runtime_error("Failed to open wave bank: " + path +
				" is a streaming bank, which this backend cannot play. Re-pack "
				"it in-memory, without xwbtool -s.");
		}
		return bank;
	}

	// play_wave below the seam: the bounds check every backend makes, with the
	// same throw, then the drop audio_device.h promises for a one-shot asked
	// for while suspended, then DirectXTK's Play.
	template<typename Format, typename Bank>
	void play_one_shot(Bank& bank, const DeviceRecovery& recovery,
		int wave_index, float volume, float pitch, float pan)
	{
		if (!holds_wave<Format>(bank, wave_index))
		{
			throw std::out_of_range(
				"A wave was played that the bank does not hold.");
		}
		if (recovery.suspended())
		{
			return;
		}
		bank.Play(static_cast<unsigned int>(wave_index), volume, pitch, pan);
	}
}
