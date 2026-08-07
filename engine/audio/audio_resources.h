#ifndef AUDIO_RESOURCES_H
#define AUDIO_RESOURCES_H

#include "engine/core/registry.h"
#include "engine/audio/sound_bank.h"
#include <memory>
#include <string>

// The sound banks, cached by name. The audio counterpart to RenderResources,
// and separate from it because the module table (ARCHITECTURE.md) has render
// and audio depending on core and math alone - neither knows the other exists,
// so nothing that only draws has to compile the audio stack to do it.
class AudioResources
{
public:
	using SoundBankHandle = Handle<SoundBank>;

	AudioResources() = default;

	// Load-time. Throws std::out_of_range naming the bank if nothing loaded
	// it. Same bargain as RenderResources: resolve a name once, keep the
	// handle, and never search by name again.
	SoundBankHandle resolve_sound_bank(const std::string& sound_bank_name) const;

	// Const and non-mutating, like every other registry getter: throws
	// std::out_of_range naming the bank if it is absent or released.
	SoundBank* get_sound_bank(SoundBankHandle sound_bank) const;
	SoundBank* get_sound_bank(const std::string& sound_bank_name) const;

	void add_sound_bank(const std::string& sound_bank_name,
		std::unique_ptr<SoundBank> sound_bank);

	// There is deliberately no reset_all_sounds(). Banks are not device
	// resources, and releasing them on device loss left every live object
	// holding a freed SoundBank* - so the only teardown is the destructor,
	// where SoundBank drops its instances before the wave bank they play from.

private:
	Registry<SoundBank> _sound_banks{ "SoundBank" };
};
#endif // !AUDIO_RESOURCES_H
