#include "samples/audio/sound_board.h"

#include "engine/math/scalar.h"

#include <stdexcept>

namespace audio_sample
{
	SoundBoard::SoundBoard(labrador::SoundBank* bank) : bank_(bank)
	{
		if (this->bank_ == nullptr || !this->bank_->audible())
		{
			throw std::runtime_error("AudioSample requires the tones wave bank.");
		}
		this->ping_ = this->bank_->resolve_wave("ping");
		this->chime_ = this->bank_->resolve_wave("chime");
		this->drone_ = this->bank_->resolve_effect("drone_loop");
	}

	SoundBoard::~SoundBoard()
	{
		this->stop_loop();
	}

	void SoundBoard::play_ping() const
	{
		this->bank_->play_wave(this->ping_, this->volume_, this->pitch_, this->pan_);
	}

	void SoundBoard::play_chime() const
	{
		this->bank_->play_wave(this->chime_, this->volume_, this->pitch_, this->pan_);
	}

	void SoundBoard::start_loop() const
	{
		this->bank_->play_effect(this->drone_, true,
			this->volume_, this->pitch_, this->pan_);
	}

	void SoundBoard::toggle_pause() const
	{
		if (this->loop_state() == labrador::SoundState::playing)
		{
			this->bank_->pause_effect(this->drone_);
		}
		else if (this->loop_state() == labrador::SoundState::paused)
		{
			this->bank_->resume_effect(this->drone_);
		}
	}

	void SoundBoard::stop_loop() const
	{
		this->bank_->stop_effect(this->drone_, true);
	}

	void SoundBoard::set_volume(float volume)
	{
		this->volume_ = mattmath::clamp(volume, 0.0f, 1.0f);
		this->bank_->set_effect_volume(this->drone_, this->volume_);
	}

	void SoundBoard::set_pan(float pan)
	{
		this->pan_ = mattmath::clamp(pan, -1.0f, 1.0f);
		this->bank_->set_effect_pan(this->drone_, this->pan_);
	}

	void SoundBoard::set_pitch(float pitch)
	{
		this->pitch_ = mattmath::clamp(pitch, -1.0f, 1.0f);
		this->bank_->set_effect_pitch(this->drone_, this->pitch_);
	}

	float SoundBoard::volume() const { return this->volume_; }
	float SoundBoard::pan() const { return this->pan_; }
	float SoundBoard::pitch() const { return this->pitch_; }

	labrador::SoundState SoundBoard::loop_state() const
	{
		return this->bank_->effect_state(this->drone_);
	}

	bool SoundBoard::is_looping() const
	{
		return this->bank_->is_effect_looping(this->drone_);
	}
}
