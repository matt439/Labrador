#ifndef SOUND_BANK_LOADER_H
#define SOUND_BANK_LOADER_H

#include "engine/audio/sound_bank.h"
#include <Audio.h>
#include <memory>

namespace sound_bank_loader
{
	// Reads the bank definition at `json_path` and builds a SoundBank playing
	// from `wave_bank`, whose ownership it takes. The effect instances are
	// created here, against the wave bank, before the SoundBank exists - which
	// is why the wave bank is handed in rather than fetched back out.
	//
	// Throws std::runtime_error naming the path if the file cannot be read or
	// parsed, and std::out_of_range naming the wave if a definition names one
	// the bank does not contain (T6).
	std::unique_ptr<SoundBank> load(const char* json_path,
		std::unique_ptr<DirectX::WaveBank> wave_bank);
}

#endif // !SOUND_BANK_LOADER_H
