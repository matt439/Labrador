#include "engine/audio/sound_bank.h"

#include "engine/audio/audio_device.h"
#include "engine/math/scalar.h"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace mattmath;

namespace labrador
{
	SoundBank::SoundBank(const AudioDevice* device,
		AudioDevice::WaveBankHandle wave_bank,
		Registry<SoundEffect> effects) :
		device_(device),
		wave_bank_(wave_bank),
		effects_(std::move(effects))
	{

	}

	std::unique_ptr<SoundBank> SoundBank::silent()
	{
		return std::unique_ptr<SoundBank>(new SoundBank(nullptr,
			AudioDevice::WaveBankHandle(), Registry<SoundEffect>("SoundEffect")));
	}

	bool SoundBank::audible() const
	{
		return this->device_ != nullptr;
	}

	SoundBank::WaveHandle SoundBank::resolve_wave(
		const std::string& wave_name) const
	{
		if (!this->audible())
		{
			// Any name, and index 0 rather than an unresolved handle: every
			// play below rejects an unresolved handle in every build now, and a
			// bank with no content must not reject a name.
			return WaveHandle(0);
		}

		// The seam answers -1 for a name a bank does not have, which is also
		// what an unresolved Handle holds - so this has to be caught here
		// rather than handed on as a handle that looks fine until it is read.
		const int index =
			this->device_->wave_index(this->wave_bank_, wave_name);
		if (index < 0)
		{
			throw std::out_of_range(
				"Wave '" + wave_name + "' is not in this sound bank.");
		}
		return WaveHandle(index);
	}

	SoundBank::EffectHandle SoundBank::resolve_effect(
		const std::string& effect_name) const
	{
		if (!this->audible())
		{
			return EffectHandle(0);
		}
		return this->effects_.resolve(effect_name);
	}

	void SoundBank::set_wave_group(WaveHandle wave, SoundGroupHandle group)
	{
		if (!wave.valid())
		{
			throw std::out_of_range(
				"A wave was given a group through an unresolved handle.");
		}
		this->check_group(group);

		if (!this->audible())
		{
			// Validated and discarded: every wave of a bank with no content is
			// wave 0, so there is no one wave to keep an assignment for.
			return;
		}

		const size_t index = static_cast<size_t>(wave.index());
		if (index >= this->wave_groups_.size())
		{
			this->wave_groups_.resize(index + 1);
		}
		this->wave_groups_[index] = group;
	}

	void SoundBank::set_effect_group(EffectHandle effect,
		SoundGroupHandle group)
	{
		// The voice is not needed, only the refusal of an unresolved handle
		// that comes with it.
		static_cast<void>(this->voice(effect));
		this->check_group(group);

		if (!this->audible())
		{
			return;
		}

		this->effects_.get(effect)->group = group;

		// Kept in handle order and once each, so a group change walks the
		// effects in the order they were defined in.
		const std::vector<EffectHandle>::iterator at = std::lower_bound(
			this->grouped_effects_.begin(), this->grouped_effects_.end(),
			effect, [](EffectHandle a, EffectHandle b)
			{
				return a.index() < b.index();
			});
		if (at == this->grouped_effects_.end() || *at != effect)
		{
			this->grouped_effects_.insert(at, effect);
		}
	}

	void SoundBank::play_wave(WaveHandle wave, float volume, float pitch,
		float pan) const
	{
		// THE CLAMP IS FIRST AND THAT IS THE POINT. Folding a level back into
		// range is arithmetic this module owns - it is not XAudio2's and it is
		// not any backend's, and it is the same kind of engine-side decision as
		// the glyph walk in render/font.h. Below the test for content it would
		// never execute at all in a tree that cannot build an audible bank,
		// which is every clone of this one.
		clamp_levels(volume, pitch, pan);

		if (!wave.valid())
		{
			throw std::out_of_range(
				"A wave was played through an unresolved handle.");
		}

		if (!this->audible())
		{
			return;
		}

		const size_t index = static_cast<size_t>(wave.index());
		const float group_volume = index < this->wave_groups_.size() ?
			this->group_volume(this->wave_groups_[index]) : 1.0f;
		this->device_->play_wave(this->wave_bank_, wave.index(),
			volume * group_volume, pitch, pan);
	}

	void SoundBank::play_effect(EffectHandle effect, bool loop, float volume,
		float pitch, float pan) const
	{
		clamp_levels(volume, pitch, pan);
		const AudioDevice::VoiceHandle voice = this->voice(effect);
		if (!this->audible())
		{
			return;
		}

		SoundEffect* record = this->effects_.get(effect);
		record->volume = volume;
		this->device_->play_voice(voice, loop,
			volume * this->group_volume(record->group), pitch, pan);
	}
	void SoundBank::stop_effect(EffectHandle effect, bool immediate) const
	{
		const AudioDevice::VoiceHandle voice = this->voice(effect);
		if (!this->audible())
		{
			return;
		}

		this->device_->stop_voice(voice, immediate);
	}
	void SoundBank::pause_effect(EffectHandle effect) const
	{
		const AudioDevice::VoiceHandle voice = this->voice(effect);
		if (!this->audible())
		{
			return;
		}

		this->device_->pause_voice(voice);
	}
	void SoundBank::resume_effect(EffectHandle effect) const
	{
		const AudioDevice::VoiceHandle voice = this->voice(effect);
		if (!this->audible())
		{
			return;
		}

		this->device_->resume_voice(voice);
	}
	void SoundBank::set_effect_volume(EffectHandle effect, float volume) const
	{
		volume = clamp(volume, 0.0f, 1.0f);
		const AudioDevice::VoiceHandle voice = this->voice(effect);
		if (!this->audible())
		{
			return;
		}

		SoundEffect* record = this->effects_.get(effect);
		record->volume = volume;
		this->device_->set_voice_volume(voice,
			volume * this->group_volume(record->group));
	}
	void SoundBank::set_effect_pitch(EffectHandle effect, float pitch) const
	{
		pitch = clamp(pitch, -1.0f, 1.0f);
		const AudioDevice::VoiceHandle voice = this->voice(effect);
		if (!this->audible())
		{
			return;
		}

		this->device_->set_voice_pitch(voice, pitch);
	}
	void SoundBank::set_effect_pan(EffectHandle effect, float pan) const
	{
		pan = clamp(pan, -1.0f, 1.0f);
		const AudioDevice::VoiceHandle voice = this->voice(effect);
		if (!this->audible())
		{
			return;
		}

		this->device_->set_voice_pan(voice, pan);
	}
	SoundState SoundBank::effect_state(EffectHandle effect) const
	{
		const AudioDevice::VoiceHandle voice = this->voice(effect);
		if (!this->audible())
		{
			return SoundState::stopped;
		}

		return this->device_->voice_state(voice);
	}
	bool SoundBank::is_effect_looping(EffectHandle effect) const
	{
		const AudioDevice::VoiceHandle voice = this->voice(effect);
		if (!this->audible())
		{
			return false;
		}

		return this->device_->is_voice_looping(voice);
	}

	AudioDevice::VoiceHandle SoundBank::voice(EffectHandle effect) const
	{
		if (!effect.valid())
		{
			// Above the check for content, deliberately: a handle nobody
			// resolved is a program's mistake rather than a missing file, and
			// the two builds should not disagree about whether to say so.
			throw std::out_of_range(
				"A sound effect was reached through an unresolved handle.");
		}

		if (!this->audible())
		{
			return AudioDevice::VoiceHandle();
		}

		// A bounds check and an indexed load, and the registry's own throw
		// naming the effect if the handle came from another table.
		return this->effects_.get(effect)->voice;
	}

	void SoundBank::check_group(SoundGroupHandle group) const
	{
		if (this->group_volumes_ == nullptr)
		{
			throw std::logic_error("A sound was given a group by a bank that "
				"belongs to no AudioResources, which is where groups are.");
		}
		if (!group.valid() ||
			static_cast<size_t>(group.index()) >= this->group_volumes_->size())
		{
			throw std::out_of_range(
				"A sound group was reached through an unresolved handle.");
		}
	}

	float SoundBank::group_volume(SoundGroupHandle group) const
	{
		// Only a handle check_group accepted is ever stored, and a group is
		// never removed, so a valid handle always has its entry.
		if (!group.valid())
		{
			return 1.0f;
		}
		return (*this->group_volumes_)[static_cast<size_t>(group.index())];
	}

	void SoundBank::apply_group_volume(SoundGroupHandle group) const
	{
		if (!this->audible())
		{
			return;
		}

		const float volume = this->group_volume(group);
		for (const EffectHandle effect : this->grouped_effects_)
		{
			const SoundEffect* record = this->effects_.get(effect);
			if (record->group != group)
			{
				continue;
			}

			// A stopped voice takes its level at its next play_effect, so it
			// is left alone; a paused one takes it now, which the seam allows,
			// and plays at it when resumed.
			if (this->device_->voice_state(record->voice) == SoundState::stopped)
			{
				continue;
			}
			this->device_->set_voice_volume(record->voice,
				record->volume * volume);
		}
	}

	void SoundBank::clamp_levels(float& volume, float& pitch, float& pan)
	{
		volume = clamp(volume, 0.0f, 1.0f);
		pitch = clamp(pitch, -1.0f, 1.0f);
		pan = clamp(pan, -1.0f, 1.0f);
	}
}
