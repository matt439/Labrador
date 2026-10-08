#include "samples/audio/audio_state.h"

#include "engine/input/keyboard.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"

#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

using namespace labrador;
using namespace mattmath;

namespace audio_sample
{
	AudioState::AudioState(Application* app) : app_(app) {}

	void AudioState::init()
	{
		SoundBank* bank = this->app_->audio_resources()->sound_bank("tones");
		this->board_ = std::make_unique<SoundBoard>(bank);
		RenderResources* resources = this->app_->render_resources();
		const std::string font = "courier_new_bold_16";
		this->scene_.add(std::make_unique<Label>(L"AUDIO / SOUND BANK",
			font, Vector2F(48.0f, 56.0f), resources, Colour::white, 2.0f));
		this->scene_.add(std::make_unique<Label>(
			L"1  Ping       2  Chime       3  Start drone loop\n\n"
			L"Space  Pause / resume       S  Stop loop\n\n"
			L"Up / Down  Volume          Left / Right  Stereo pan\n\n"
			L"Q / E  Pitch               Escape  Quit",
			font, Vector2F(48.0f, 144.0f), resources));
		this->status_ = this->scene_.add(std::make_unique<Label>(L"",
			font, Vector2F(48.0f, 388.0f), resources));
#if defined(LABRADOR_AUDIO_SAMPLE_NULL)
		const std::wstring backend =
			L"Null audio: calls are recorded. Use x64-debug to hear these tones.";
#else
		const std::wstring backend =
			L"XAudio2: original PCM tones. Headphones reveal left / right pan.";
#endif
		this->scene_.add(std::make_unique<Label>(backend,
			font, Vector2F(48.0f, 540.0f), resources));
		this->scene_.add(std::make_unique<Label>(
			L"One-shots overlap. The drone is one persistent voice.\n"
			L"Adjustments change the drone and the next one-shot.",
			font, Vector2F(48.0f, 588.0f), resources));
		this->scene_.end_tick();
		this->scene_.add_view(Viewport(RectangleF(Vector2F::ZERO,
			this->app_->resolution_manager()->resolution_vec())));
		this->refresh_status();
	}

	void AudioState::update(float dt)
	{
		const Keyboard& keyboard = *this->app_->keyboard();
		if (keyboard.pressed(Key::escape)) { this->app_->quit(); }
		if (keyboard.pressed(Key::digit_1)) { this->board_->play_ping(); }
		if (keyboard.pressed(Key::digit_2)) { this->board_->play_chime(); }
		if (keyboard.pressed(Key::digit_3)) { this->board_->start_loop(); }
		if (keyboard.pressed(Key::space)) { this->board_->toggle_pause(); }
		if (keyboard.pressed(Key::s)) { this->board_->stop_loop(); }

		const float volume_direction = static_cast<float>(keyboard.held(Key::up)) -
			static_cast<float>(keyboard.held(Key::down));
		const float pan_direction = static_cast<float>(keyboard.held(Key::right)) -
			static_cast<float>(keyboard.held(Key::left));
		const float pitch_direction = static_cast<float>(keyboard.held(Key::e)) -
			static_cast<float>(keyboard.held(Key::q));
		if (volume_direction != 0.0f)
		{
			this->board_->set_volume(this->board_->volume() + volume_direction * dt);
		}
		if (pan_direction != 0.0f)
		{
			this->board_->set_pan(this->board_->pan() + pan_direction * dt);
		}
		if (pitch_direction != 0.0f)
		{
			this->board_->set_pitch(this->board_->pitch() + pitch_direction * dt);
		}
		this->refresh_status();
		this->scene_.update(dt);
		this->scene_.end_tick();
	}

	void AudioState::refresh_status()
	{
		const SoundState state = this->board_->loop_state();
		const wchar_t* name = state == SoundState::playing ? L"playing" :
			state == SoundState::paused ? L"paused" : L"stopped";
		std::wostringstream text;
		text << L"Drone: " << name << std::fixed << std::setprecision(2)
			<< L"\n\nVolume: " << this->board_->volume()
			<< L"    Pan: " << this->board_->pan()
			<< L"    Pitch: " << this->board_->pitch();
		this->status_->set_text(text.str());
	}

	void AudioState::draw(Renderer& renderer) const
	{
		this->scene_.draw(renderer);
	}

	void AudioState::on_deactivated()
	{
		this->resume_loop_ = this->board_->loop_state() == SoundState::playing;
		if (this->resume_loop_) { this->board_->toggle_pause(); }
	}

	void AudioState::on_activated()
	{
		if (this->resume_loop_) { this->board_->toggle_pause(); }
		this->resume_loop_ = false;
	}
}
