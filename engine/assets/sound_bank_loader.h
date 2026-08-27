#pragma once

#include "engine/audio/audio_device.h"
#include "engine/audio/sound_bank.h"

#include <memory>
#include <string>
#include <vector>

namespace labrador
{
	// A bank definition, parsed: what the content says a bank holds, before
	// anything has been opened.
	//
	// IT IS A TYPE RATHER THAN A LOCAL BECAUSE THE SEAM NEEDS IT FIRST. A
	// backend with no container - engine/audio/null/ - has no name table of its
	// own and answers wave_index out of this list, so the definition has to be
	// read before AudioDevice::open_wave_bank is called rather than after it.
	// That inverts the old order, where the wave bank was constructed first and
	// the JSON read against it, and it is the one behaviour change a reader
	// might notice: a bank whose container is missing AND whose definition is
	// malformed now reports the malformed definition. It is the better of the
	// two answers - the definition is in every clone, the container is not (T6)
	// - but it is a change and this is where it is written down.
	struct SoundBankDefinition
	{
		// Every wave the definition names, in the order it names them and with
		// no duplicates: the `waves` array first, then any wave a sound effect
		// instance names that the array did not list.
		std::vector<std::string> waves;

		// One named effect instance and the wave it plays.
		struct Effect
		{
			std::string name;
			std::string wave;
		};

		// In creation order, which is handle order: the per-wave instances
		// first if the definition asked for them, then the explicit ones.
		std::vector<Effect> effects;

		// Where this was read from, so the throws below can name it.
		std::string source_path;
	};

	// Reads the bank definition at `json_path`.
	//
	// Throws std::runtime_error naming the path if the file cannot be read or
	// parsed, is the wrong shape, or names two effect instances the same (T6).
	SoundBankDefinition read_sound_bank_definition(const char* json_path);

	// Builds the SoundBank that plays `wave_bank` out of `device`, creating one
	// voice per effect the definition names.
	//
	// The voices are created here, against a bank the caller has already
	// opened, which is why the handle is passed in rather than fetched back
	// out. Throws std::out_of_range naming the definition and the wave if the
	// bank does not hold one an effect names - which, on a backend that has a
	// container, open_wave_bank will already have said first.
	std::unique_ptr<SoundBank> build_sound_bank(AudioDevice* device,
		AudioDevice::WaveBankHandle wave_bank,
		const SoundBankDefinition& definition);
}
