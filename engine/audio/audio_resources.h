#pragma once

#include "engine/core/registry.h"
#include "engine/audio/sound_bank.h"
#include "engine/audio/sound_group.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace labrador
{
	// The sound banks, cached by name, and the sound groups whose volumes
	// they mix in. The audio counterpart to RenderResources, and separate from
	// it because the module table (ARCHITECTURE.md) has render and audio
	// depending on core and math alone - neither knows the other exists, so
	// nothing that only draws has to compile the audio stack to do it.
	class AudioResources
	{
	public:
		using SoundBankHandle = Handle<SoundBank>;
		using SoundGroupHandle = labrador::SoundGroupHandle;

		AudioResources() = default;

		// Load-time. Throws std::out_of_range naming the bank if nothing loaded
		// it. Same bargain as RenderResources: resolve a name once, keep the
		// handle, and never search by name again.
		SoundBankHandle resolve_sound_bank(const std::string& sound_bank_name) const;

		// Const and non-mutating, like every other registry getter: throws
		// std::out_of_range naming the bank if it is absent or released.
		// The returned pointer is borrowed until this table is destroyed or
		// add_sound_bank replaces or empties that name's slot. Keep a handle,
		// not this pointer, across such a change and read the slot again.
		SoundBank* sound_bank(SoundBankHandle sound_bank) const;
		SoundBank* sound_bank(const std::string& sound_bank_name) const;

		// Takes ownership. Re-adding a name destroys its previous bank and
		// refills the same slot; a null bank empties it. Existing handles keep
		// naming that slot, and sound_bank throws while it is empty. Pointers
		// returned before replacement or emptying must no longer be used.
		//
		// Also lends the bank this object's group volumes, which is what lets
		// its set_wave_group and set_effect_group accept a group. The bank and
		// the volumes are both owned here, and the banks are destroyed first,
		// so no bank outlives the volumes it was lent.
		void add_sound_bank(const std::string& sound_bank_name,
			std::unique_ptr<SoundBank> sound_bank);

		// Banks are not device resources, so a device loss leaves them alone.
		// A bank holds only handles: the voices and the wave bank behind them
		// belong to the AudioDevice, which releases them itself. This does not
		// preserve a bank pointer across add_sound_bank replacing its name.

		// SOUND GROUPS. A group is a volume a game sets once for many sounds -
		// the music slider and the effects slider of a settings screen. A bank
		// is told at load which of its waves and effects belong to which group
		// (SoundBank::set_wave_group, set_effect_group), and from then on every
		// level it sends to the device is the level the caller asked for times
		// its group's volume. A sound in no group is at full group volume, so a
		// game that never adds a group hears exactly what it asked for.
		//
		// What a group is for, which sounds are in it and what its volume is
		// are the game's; so are fades, ducking and authored levels, which a
		// game builds from these calls and set_effect_volume (T1). A group
		// spans banks, which is why it lives here rather than in one of them.

		// Load-time. Starts at volume 1. Throws std::invalid_argument naming the
		// group if one of that name was already added. If storing a new group
		// throws, its name remains absent and can be retried; existing groups
		// and their volumes are unchanged.
		SoundGroupHandle add_sound_group(const std::string& group_name);

		// Load-time. Throws std::out_of_range naming the group if none of that
		// name was added.
		SoundGroupHandle resolve_sound_group(const std::string& group_name) const;

		// Clamped to [0,1]. Throws std::out_of_range through an unresolved
		// handle.
		//
		// A one-shot already playing keeps the level it was launched with, and
		// one launched after this call takes the new level. A retained effect
		// in the group, across every bank this holds, takes it now if it is
		// playing or paused - a paused one plays at it when resumed - and a
		// stopped one takes it at its next play_effect. Each of those pushes is
		// one call to the device, made here and never per frame.
		void set_group_volume(SoundGroupHandle group, float volume);

		// Throws std::out_of_range through an unresolved handle.
		float group_volume(SoundGroupHandle group) const;

	private:
		// One volume per group, by handle index. Lent to every bank added here
		// (SoundBank reads it, never writes it), and on the heap so that the
		// address the banks hold does not move when this object does. Declared
		// before sound_banks_, and the order is load-bearing: members are
		// destroyed in reverse, so every bank is destroyed before the volumes
		// it was lent.
		std::unique_ptr<std::vector<float>> group_volumes_ =
			std::make_unique<std::vector<float>>();

		Registry<SoundBank> sound_banks_{ "SoundBank" };

		// Every bank's name, in the order it was first added, so a group's
		// volume can reach every bank: Registry has no iteration. A name rather
		// than a pointer, because re-adding a name replaces the bank behind it.
		std::vector<std::string> bank_order_;

		std::map<std::string, int> group_indices_;
	};
}
