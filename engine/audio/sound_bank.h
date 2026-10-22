#pragma once

#include "engine/audio/audio_device.h"
#include "engine/audio/sound_group.h"
#include "engine/core/registry.h"

#include <memory>
#include <string>
#include <vector>

namespace labrador
{
	// A wave bank and the effect instances built over it, both reached by
	// handle.
	//
	// Names resolve once and are never seen again (PHILOSOPHY T7, T8). That is
	// worth more here than it looks: an audio API searches a bank's name table
	// on every play-by-name, and DirectXTK's WaveBank writes a debug trace and
	// plays nothing on a miss - so a misspelt wave played by name is silence
	// with no error at all. Resolved up front, the same typo throws at load,
	// naming the wave (T6).
	//
	// WHAT IS ON THIS SIDE OF THE SEAM. Everything in this class is engine
	// arithmetic and engine policy: which name means which wave, which handles
	// are valid, and the clamp that folds a volume into [0,1] and a pitch and a
	// pan into [-1,1]. It calls audio_device.h for the four or five things an
	// audio API actually does.
	//
	// Nothing here may name one. A bank holding a DirectX::WaveBank and a
	// Registry<DirectX::SoundEffectInstance>, with its effect handle and its
	// effect_state spelt in a library's namespace, makes a game name XAudio2 to
	// ask what its music is doing.
	//
	// AND THE ORDER INSIDE EACH METHOD IS THE OTHER HALF OF IT. The clamp and
	// the unresolved-handle check happen ABOVE the test for whether this bank
	// has anything behind it, not below. The arithmetic is this module's, so
	// it belongs on this module's side of the wall, and it runs whether or not
	// there is a sound at the end of it - so a silent bank exercises it as
	// fully as an audible one, and a build that cannot open a wave bank can
	// still test it.
	//
	// AUDIO CALLS ARE update()-SIDE. Several of the methods below are const
	// and still write the bank's own record of an effect's level, and draw()
	// is entered concurrently on the same object under the view fan-out - so
	// nothing here is called from a draw().
	class SoundBank
	{
	public:
		// A wave in the bank. There is no type for one - a wave is an index
		// into whatever container the backend opened - so this tag exists only
		// to give the handle something to be about, and is deliberately never
		// defined.
		struct Wave;
		using WaveHandle = Handle<Wave>;

		// The engine's record of one named effect instance: which voice the
		// device built for it, which sound group it is in, and the last level
		// a caller asked for, after clamping and before the group's volume is
		// multiplied in - kept so a group volume change can push the product.
		//
		// It is a struct rather than the voice handle on its own so that the
		// effect table can be a Registry like every other table in this engine
		// - one lookup contract, one throw naming what was missing, and the
		// same bargain audio_resources.h claims for banks (core/registry.h).
		struct SoundEffect
		{
			AudioDevice::VoiceHandle voice;
			SoundGroupHandle group{};
			float volume = 1.0f;
		};
		using EffectHandle = Registry<SoundEffect>::handle;

		// Built by build_sound_bank (engine/assets/) from an already-parsed
		// definition and an already-opened bank: this class plays what it was
		// handed, it does not read files.
		//
		// `device` is borrowed and must outlive the bank. It is the same
		// bargain RenderResources makes with the renderer, and the shell
		// declares them in the order that keeps it (application.h).
		SoundBank(const AudioDevice* device,
			AudioDevice::WaveBankHandle wave_bank,
			Registry<SoundEffect> effects);

		// A bank with nothing in it, for content that is not there.
		//
		// IT IS NOT THE HEADLESS IMPLEMENTATION OF THE SEAM. That is
		// engine/audio/null/, chosen at build time, and it records what it was
		// asked to play. This is the answer to a different question - a wave
		// bank that is not on disk at run time. A bank built from source audio
		// that cannot be distributed is not in a fresh clone, so the manifest
		// marks it optional, and this is what the loader puts in its place:
		// every resolve succeeds, every play does nothing, and a game that is
		// otherwise correct runs in silence rather than throwing at startup on
		// a file it was never going to have.
		//
		// WHAT IT COSTS, SAID OUT LOUD. Resolving is this class's whole T6
		// guarantee - a misspelt wave name throws at load rather than going
		// quiet at the moment it should have been heard. A bank with no content
		// cannot keep that promise, because it has no name table to check
		// against. So the trade is: with the bank present, a typo is a startup
		// failure; with it absent, it is nothing, because everything is. That
		// is why this is a named constructor a loader has to ask for and not a
		// state the class can fall into.
		//
		// AND IT STILL REFUSES A BAD HANDLE. The check for the handle sits above
		// the check for content, not below it: a name always resolves, and a
		// handle that was never resolved is refused by every bank. The other
		// order makes the one mistake this class exists to catch loudly depend on
		// whether the build has audio in it.
		static std::unique_ptr<SoundBank> silent();

		// Whether this bank has content behind it. False only for silent().
		//
		// IT IS NOT A QUESTION ABOUT THE BUILD, and the two are easily confused.
		// Under the null audio backend every bank here is
		// audible and none of them makes a noise: whether a device puts a wave
		// through a speaker is chosen in CMake and is not a property of a bank.
		// What this answers is narrower and is the only thing a caller can
		// usefully do with it - was there a wave bank to open, or is this the
		// substitute a missing one produced.
		bool audible() const;

		// Load-time. Each throws std::out_of_range naming what was asked for if
		// this bank does not have it, so a bad name fails where it is written
		// rather than going quiet at the moment it should have been heard.
		//
		// The two are different types on purpose. A wave index and an effect
		// index are both an int and index different tables, and playing wave 3
		// when effect 3 was meant is the kind of mistake that sounds almost
		// right.
		WaveHandle resolve_wave(const std::string& wave_name) const;
		EffectHandle resolve_effect(const std::string& effect_name) const;

		// Load-time. Puts a wave or an effect in a sound group of the
		// AudioResources this bank was added to, and from then on every level
		// sent to the device for it is multiplied by that group's volume
		// (audio_resources.h). Assigning again moves it; nothing takes one out.
		// An effect takes its group's volume at its next play_effect,
		// set_effect_volume or group volume change.
		//
		// Each throws std::out_of_range through an unresolved wave, effect or
		// group handle, std::logic_error if this bank was never added to an
		// AudioResources - it has no groups to put anything in - and checks
		// all of that above the check for content. A bank with no content
		// validates and discards: it has no record to keep an assignment on.
		void set_wave_group(WaveHandle wave, SoundGroupHandle group);
		void set_effect_group(EffectHandle effect, SoundGroupHandle group);

		// Fire-and-forget: a new voice each time, with no handle on it
		// afterwards. This is the one for a jump, a hit, a menu click. It
		// launches at `volume` times its group's volume, and keeps that level
		// while it plays: a group volume change reaches the next one.
		void play_wave(WaveHandle wave, float volume = 1.0f,
			float pitch = 0.0f, float pan = 0.0f) const;

		// An effect instance is a voice that persists, so it can be looped,
		// stopped, and adjusted while it plays - music, a weapon's firing loop.
		// Each throws std::out_of_range through an unresolved handle.
		//
		// play_effect and set_effect_volume send `volume` times the effect's
		// group volume, and keep `volume` in the effect's record through the
		// registry's non-const pointer, which is why they are const and still
		// write.
		void play_effect(EffectHandle effect, bool loop = false,
			float volume = 1.0f, float pitch = 0.0f, float pan = 0.0f) const;
		void stop_effect(EffectHandle effect, bool immediate = false) const;
		void pause_effect(EffectHandle effect) const;
		void resume_effect(EffectHandle effect) const;
		void set_effect_volume(EffectHandle effect, float volume) const;
		void set_effect_pitch(EffectHandle effect, float pitch) const;
		void set_effect_pan(EffectHandle effect, float pan) const;
		SoundState effect_state(EffectHandle effect) const;
		bool is_effect_looping(EffectHandle effect) const;

	private:
		friend class AudioResources;

		// Borrowed, and null for a silent bank - which is what audible() reads.
		const AudioDevice* device_ = nullptr;

		AudioDevice::WaveBankHandle wave_bank_;
		Registry<SoundEffect> effects_{ "SoundEffect" };

		// Each group's volume by handle index, lent by the AudioResources this
		// bank was added to, which owns both and destroys its banks before
		// the volumes. Null for a bank no AudioResources holds.
		const std::vector<float>* group_volumes_ = nullptr;

		// A wave's group by wave index, grown by set_wave_group as far as the
		// highest wave assigned. A wave past its end is in no group.
		std::vector<SoundGroupHandle> wave_groups_;

		// The effects given a group, in handle order: what a group volume
		// change walks, because the effect registry cannot be iterated.
		std::vector<EffectHandle> grouped_effects_;

		// Throws std::logic_error with no group table, std::out_of_range for
		// a handle it has no entry for.
		void check_group(SoundGroupHandle group) const;

		// The volume of a group an assignment stored, and 1 for no group.
		float group_volume(SoundGroupHandle group) const;

		// AudioResources' half of set_group_volume: pushes the new product to
		// every playing or paused effect in `group`.
		void apply_group_volume(SoundGroupHandle group) const;

		// The voice behind a named effect, and the engine half of every call
		// that takes one.
		//
		// It refuses an unresolved handle before it asks whether there is
		// anything to play, so that refusal is the same in every build; and it
		// answers with an unresolved voice for a silent bank, because a bank
		// that resolved every name must not then reject the handle it issued.
		AudioDevice::VoiceHandle voice(EffectHandle effect) const;

		static void clamp_levels(float& volume, float& pitch, float& pan);
	};
}
