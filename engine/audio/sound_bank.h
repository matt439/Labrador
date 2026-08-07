#pragma once

#include <Audio.h>
#include <memory>
#include <string>
#include "engine/core/registry.h"
#include "engine/math/matt_math.h"

// A wave bank and the effect instances built over it, both reached by handle.
//
// Names resolve once and are never seen again (PHILOSOPHY T7, T8). That is
// worth more here than it looks: DirectXTK's WaveBank::Play(name) searches the
// bank's name table on every call, and on a miss it writes a debug trace and
// plays nothing - so a misspelt wave was silence with no error at all. Resolved
// up front, the same typo throws at load, naming the wave (T6).
class SoundBank
{
public:
	// A wave in the bank. DirectXTK has no type for one - a wave is an index
	// into the .xwb - so this tag exists only to give the handle something to
	// be about, and is deliberately never defined.
	struct Wave;
	using WaveHandle = Handle<Wave>;
	using EffectHandle = Registry<DirectX::SoundEffectInstance>::handle;

	// Built by sound_bank_loader (engine/assets/) from an already-parsed
	// definition: the bank plays what it was handed, it does not read files.
	SoundBank(std::unique_ptr<DirectX::WaveBank> wave_bank,
		Registry<DirectX::SoundEffectInstance> instances);

	// Load-time. Each throws std::out_of_range naming what was asked for if
	// this bank does not have it, so a bad name fails where it is written
	// rather than going quiet at the moment it should have been heard.
	//
	// The two are different types on purpose. A wave index and an effect index
	// are both an int and index different tables, and playing wave 3 when
	// effect 3 was meant is the kind of mistake that sounds almost right.
	WaveHandle resolve_wave(const std::string& wave_name) const;
	EffectHandle resolve_effect(const std::string& effect_name) const;

	// Fire-and-forget: a new voice each time, with no handle on it afterwards.
	// This is the one for a jump, a hit, a menu click.
	void play_wave(WaveHandle wave, float volume = 1.0f,
		float pitch = 0.0f, float pan = 0.0f) const;

	// An effect instance is a voice that persists, so it can be looped,
	// stopped, and adjusted while it plays - music, a weapon's firing loop.
	// Each throws std::out_of_range through an unresolved handle.
	void play_effect(EffectHandle effect, bool loop = false, float volume = 1.0f,
		float pitch = 0.0f, float pan = 0.0f) const;
	void stop_effect(EffectHandle effect, bool immediate = false) const;
	void pause_effect(EffectHandle effect) const;
	void resume_effect(EffectHandle effect) const;
	void set_effect_volume(EffectHandle effect, float volume) const;
	void set_effect_pitch(EffectHandle effect, float pitch) const;
	void set_effect_pan(EffectHandle effect, float pan) const;
	DirectX::SoundState effect_state(EffectHandle effect) const;
	bool is_effect_looping(EffectHandle effect) const;

private:
	std::unique_ptr<DirectX::WaveBank> wave_bank_ = nullptr;
	Registry<DirectX::SoundEffectInstance> sound_effect_instances_;

	// The raw DirectXTK voice, for this class alone. It used to be public, and
	// handing one out let a caller do anything to a voice the bank believes it
	// owns - including outliving it.
	DirectX::SoundEffectInstance* sound_effect_instance(
		EffectHandle effect) const;

	static void clamp_levels(float& volume, float& pitch, float& pan);
};
