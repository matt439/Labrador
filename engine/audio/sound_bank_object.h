#pragma once

#include "engine/audio/sound_bank.h"
#include "engine/audio/audio_resources.h"
#include <string>

namespace labrador
{
	// Anything that makes noise, inherited for the plumbing: it holds the bank
	// handle and forwards the calls, so a subclass writes play_wave(x) instead of
	// audio_resources->sound_bank(handle)->play_wave(x).
	//
	// Names go in at construction and handles come out; the play calls take
	// handles only. A subclass therefore resolves what it can make a noise with
	// once, in its constructor, and keeps the handles as members - which is why
	// the resolve helpers are here and the name-taking play calls are not.
	class SoundBankObject
	{
	public:
		// A subclass that has not been given an AudioResources, which every
		// object drawn by this engine has to be capable of being: draw objects
		// are default-constructible. Nothing can fill the table in afterwards -
		// set_sound_bank changes which bank, not which table it comes out of -
		// so an object built this way is permanently unusable, and both ways of
		// touching it throw std::logic_error saying so. That is T6 rather than
		// the null dereference in somebody else's frame it used to be.
		SoundBankObject() = default;

		// Throws std::out_of_range naming the bank if nothing loaded it, and
		// std::logic_error for a null table.
		SoundBankObject(const std::string& sound_bank_name,
			const AudioResources* audio_resources);
	protected:
		SoundBank* sound_bank() const;

		// Load-time. Each throws std::out_of_range naming what was asked for if
		// the bank does not have it, so a misspelt sound fails while the menu is
		// being built rather than staying silent on the press that wanted it.
		SoundBank::WaveHandle resolve_wave(const std::string& wave_name) const;
		SoundBank::EffectHandle resolve_effect(const std::string& effect_name) const;

		void play_wave(SoundBank::WaveHandle wave,
			float volume = 1.0f, float pitch = 0.0f, float pan = 0.0f) const;
		void play_effect(SoundBank::EffectHandle effect,
			bool loop = false, float volume = 1.0f, float pitch = 0.0f,
			float pan = 0.0f) const;
		void stop_effect(SoundBank::EffectHandle effect,
			bool immediate = false) const;
		void pause_effect(SoundBank::EffectHandle effect) const;
		void resume_effect(SoundBank::EffectHandle effect) const;
		void set_effect_volume(SoundBank::EffectHandle effect, float volume) const;
		void set_effect_pitch(SoundBank::EffectHandle effect, float pitch) const;
		void set_effect_pan(SoundBank::EffectHandle effect, float pan) const;
		SoundState effect_state(SoundBank::EffectHandle effect) const;
		bool is_effect_looping(SoundBank::EffectHandle effect) const;

		// Points this object at a different bank. Every handle resolved from the
		// old one is meaningless against the new one, so a caller doing this
		// re-resolves everything it holds.
		void set_sound_bank(const std::string& sound_bank_name);
	private:
		// The bank name is resolved once, at construction, and so is everything
		// played out of it: a handle is an index, where a name is a map descent
		// and a string compare per node.
		AudioResources::SoundBankHandle sound_bank_;
		const AudioResources* audio_resources_ = nullptr;
	};
}
