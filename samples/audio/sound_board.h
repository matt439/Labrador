#pragma once

#include "engine/audio/sound_bank.h"

namespace audio_sample
{
	// Borrows a loaded bank. All names are resolved before the first input.
	class SoundBoard
	{
	public:
		explicit SoundBoard(labrador::SoundBank* bank);
		~SoundBoard();

		SoundBoard(const SoundBoard&) = delete;
		SoundBoard& operator=(const SoundBoard&) = delete;

		void play_ping() const;
		void play_chime() const;
		void start_loop() const;
		void toggle_pause() const;
		void stop_loop() const;
		void set_volume(float volume);
		void set_pan(float pan);
		void set_pitch(float pitch);

		float volume() const;
		float pan() const;
		float pitch() const;
		labrador::SoundState loop_state() const;
		bool is_looping() const;

	private:
		// The caller keeps the bank alive through this board's destructor.
		labrador::SoundBank* bank_;
		labrador::SoundBank::WaveHandle ping_;
		labrador::SoundBank::WaveHandle chime_;
		labrador::SoundBank::EffectHandle drone_;
		float volume_ = 0.4f;
		float pan_ = 0.0f;
		float pitch_ = 0.0f;
	};
}
