#include "engine/audio/audio_device.h"
#include "engine/audio/xwb_file.h"

#include <emscripten/emscripten.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// The Web Audio backend: the browser's AudioContext behind the seam, and the
// only file in this repository that names it.
//
// WHAT THIS FILE OWNS THAT THE SEAM DOES NOT. Five things, the first of them
// the one xaudio2/ also lists and the rest the browser's:
//
//  - THE CONTAINER. A wave bank is an .xwb, read by the engine's own reader
//    (engine/audio/xwb_file.h), because Web Audio has no reader for it. Every
//    wave arrives as 16-bit PCM and becomes an AudioBuffer of floats at load,
//    so nothing is decoded while a sound plays and the samples the engine read
//    are dropped once the browser holds them. A wave at a rate the browser's
//    AudioBuffer refuses fails the load with a runtime_error naming it.
//  - THE NODE GRAPH. A voice is an AudioBufferSourceNode through a GainNode
//    and a StereoPannerNode, the three levels the seam hands over. Pitch is
//    DirectXTK's: an octave either way at the ends of [-1,1], so the rate is
//    two to the power of the pitch.
//  - PAUSE, WHICH WEB AUDIO DOES NOT HAVE. A source node plays once and is
//    thrown away. A pause here records where the voice had got to, by the
//    context's clock and the rate it was playing at, and a resume starts a new
//    node from there; a pitch change while playing rebases that arithmetic so
//    the position stays true. It is the one piece of bookkeeping XAudio2 does
//    for its caller and this API does not.
//  - THE GESTURE. A browser lets a page make sound only after the person has
//    pressed, clicked or touched something (the autoplay policy). This backend
//    creates its AudioContext inside the first such event, or when its first
//    bank opens if the page has had one already, and before then there is
//    nothing to play through. A device that never opens a bank never makes a
//    context, so a game with no sound costs the page none. audio_device.h
//    states what the wait means for a caller, because it is a contract rather
//    than a detail: a one-shot asked for before then is dropped, and a
//    persistent voice asked to play starts when sound is allowed and answers
//    playing meanwhile. Not every key is a gesture: Escape is not, by the
//    HTML standard's list, and a context made inside its keydown is refused
//    with a console warning and left suspended. So the listener asks
//    navigator.userActivation whether the page is active at that moment, and
//    where a browser cannot say, leaves Escape out by name.
//  - NO AUDIO AT ALL, which is a page without Web Audio and is also Node, where
//    this configuration's tests run. The device then has no context and never
//    will; banks are still read and checked, every handle and every state
//    transition still behaves, and nothing sounds. It is the answer XAudio2
//    gives on a machine with no audio endpoint, and the reason the tests under
//    Node exercise this file rather than skip it.
//
// THE LISTENER IS THE BACKEND'S, ON THE WINDOW, NOT THE ENGINE WINDOW'S,
// because the alternative crosses a wall for nothing. The engine's web window
// (engine/app/web/window.cpp) already hears every key and click, but for it to
// unlock audio it would have to reach an AudioDevice - app/ may lean on
// audio/, so that is legal - and the seam would grow a verb ("the person did
// something") that two of its three backends would implement as nothing. The
// autoplay policy is a property of this platform's audio API, so its answer
// lives beside that API, in capture-phase listeners on `window` that hear the
// event whatever element it is for, which also means a click on the page
// around the canvas counts. They are registered while a device exists and
// removed when it dies.
//
// WHAT IS NOT THE SAME AS XAUDIO2, said where it is decided. A voice told to
// stop without `immediate` that is not looping plays out to the end of its
// wave and then answers stopped. XAudio2, under DirectXTK, falls silent at
// once and goes on answering playing until it is stopped immediately, because
// the stop it asks for keeps the buffer queued; this backend gives the outcome
// the seam's words describe instead, which is also the one engine/audio/null/
// describes. A non-looping voice that reaches its end answers stopped from the
// next voice_state(), exactly as DirectXTK's GetState does.

// Global rather than in labrador::, because EM_JS declares a C function, and
// prefixed for the same reason.
//
// THE JAVASCRIPT IS ONE OBJECT, Module.labradorAudio, installed once and
// shared by every device the module creates; each function below is a single
// call into it. A device is an index into its table, a bank and a voice are
// indices into the device's, and those indices are the same ones the C++ side
// hands out, because both sides push in the same order.
EM_JS(void, labrador_audio_install, (), {
	if (Module.labradorAudio) {
		return;
	}

	const Context = (typeof AudioContext !== "undefined") ? AudioContext
		: (typeof webkitAudioContext !== "undefined") ? webkitAudioContext
		: undefined;
	const usable = Context !== undefined && typeof AudioBuffer !== "undefined";
	const gestures = ["keydown", "mousedown", "pointerdown", "pointerup",
		"touchend"];

	function grants_activation(event) {
		if (typeof navigator !== "undefined" && navigator.userActivation) {
			return navigator.userActivation.isActive;
		}
		return !(event.type === "keydown" && event.key === "Escape");
	}

	function position(device, voice) {
		if (!voice.source) {
			return voice.offset;
		}
		let at = voice.offset +
			(device.context.currentTime - voice.started_at) * voice.rate;
		const wave = device.banks[voice.bank][voice.wave];
		if (voice.loop && at >= wave.loop_end && wave.loop_end > wave.loop_start) {
			at = wave.loop_start +
				(at - wave.loop_start) % (wave.loop_end - wave.loop_start);
		}
		return at;
	}

	function detach(voice) {
		const source = voice.source;
		if (!source) {
			return;
		}
		voice.source = null;
		source.onended = null;
		try {
			source.stop();
		} catch (ignored) {
		}
		source.disconnect();
		voice.gain.disconnect();
		voice.panner.disconnect();
		voice.gain = null;
		voice.panner = null;
	}

	function connect(device, wave, volume, rate, pan, loop) {
		const context = device.context;
		const source = new AudioBufferSourceNode(context, {
			buffer: wave.buffer,
			playbackRate: rate,
			loop: loop,
			loopStart: wave.has_region ? wave.loop_start : 0,
			loopEnd: wave.has_region ? wave.loop_end : 0,
		});
		const gain = new GainNode(context, { gain: volume });
		const panner = new StereoPannerNode(context, { pan: pan });
		source.connect(gain);
		gain.connect(panner);
		panner.connect(context.destination);
		return { source: source, gain: gain, panner: panner };
	}

	function start(device, voice) {
		const wave = device.banks[voice.bank][voice.wave];
		if (!device.context || voice.source) {
			return;
		}
		if (!voice.loop && voice.offset >= wave.duration) {
			voice.offset = 0;
			voice.ended = true;
			voice.active = false;
			return;
		}
		const nodes = connect(device, wave, voice.volume, voice.rate, voice.pan,
			voice.loop);
		voice.source = nodes.source;
		voice.gain = nodes.gain;
		voice.panner = nodes.panner;
		voice.started_at = device.context.currentTime;
		const source = nodes.source;
		source.onended = () => {
			if (voice.source !== source) {
				return;
			}
			detach(voice);
			voice.offset = 0;
			voice.active = false;
			voice.ended = true;
		};
		source.start(0, voice.offset);
	}

	function unlock(device) {
		if (device.banks.length === 0) {
			return;
		}
		if (!device.context) {
			device.context = new Context({ latencyHint: "interactive" });
			for (const voice of device.voices) {
				if (voice.active) {
					start(device, voice);
				}
			}
		}
		if (device.suspended) {
			device.context.suspend();
		} else if (device.context.state !== "running") {
			device.context.resume();
		}
	}

	Module.labradorAudio = {
		devices: [],

		open() {
			const device = {
				context: null,
				suspended: false,
				banks: [],
				voices: [],
				listener: null,
			};
			if (usable && typeof window !== "undefined") {
				device.listener = (event) => {
					if (grants_activation(event)) {
						unlock(device);
					}
				};
				for (const name of gestures) {
					window.addEventListener(name, device.listener, true);
				}
			}
			this.devices.push(device);
			return this.devices.length - 1;
		},

		close(id) {
			const device = this.devices[id];
			if (device.listener) {
				for (const name of gestures) {
					window.removeEventListener(name, device.listener, true);
				}
			}
			for (const voice of device.voices) {
				detach(voice);
			}
			if (device.context) {
				device.context.close();
			}
			this.devices[id] = null;
		},

		add_bank(id) {
			const device = this.devices[id];
			device.banks.push([]);
			if (device.listener && typeof navigator !== "undefined" &&
				navigator.userActivation && navigator.userActivation.hasBeenActive) {
				unlock(device);
			}
			return device.banks.length - 1;
		},

		add_wave(id, bank, channels, rate, frames, samples, loop_start,
			loop_length) {
			const device = this.devices[id];
			const wave = {
				buffer: null,
				duration: frames / rate,
				has_region: loop_length > 0,
				loop_start: loop_length > 0 ? loop_start / rate : 0,
				loop_end: loop_length > 0 ? (loop_start + loop_length) / rate
					: frames / rate,
			};
			if (usable && frames > 0) {
				try {
					wave.buffer = new AudioBuffer({
						length: frames,
						numberOfChannels: channels,
						sampleRate: rate,
					});
				} catch (refused) {
					// A rate this browser's Web Audio will not hold. The bank
					// goes with it, because it is the last one opened and the
					// load that opened it is about to throw.
					device.banks.length = bank;
					return 0;
				}
				const first = samples >> 1;
				for (let channel = 0; channel < channels; channel++) {
					const out = wave.buffer.getChannelData(channel);
					for (let frame = 0; frame < frames; frame++) {
						out[frame] = HEAP16[first + frame * channels + channel] / 32768;
					}
				}
			}
			device.banks[bank].push(wave);
			return 1;
		},

		play_wave(id, bank, index, volume, rate, pan) {
			const device = this.devices[id];
			const wave = device.banks[bank][index];
			if (!device.context || device.suspended || !wave.buffer) {
				return;
			}
			const nodes = connect(device, wave, volume, rate, pan, false);
			nodes.source.onended = () => {
				nodes.source.disconnect();
				nodes.gain.disconnect();
				nodes.panner.disconnect();
			};
			nodes.source.start();
		},

		create_voice(id, bank, index) {
			const device = this.devices[id];
			device.voices.push({
				bank: bank,
				wave: index,
				volume: 1,
				rate: 1,
				pan: 0,
				loop: false,
				active: false,
				ended: false,
				offset: 0,
				started_at: 0,
				source: null,
				gain: null,
				panner: null,
			});
			return device.voices.length - 1;
		},

		play(id, index, loop) {
			const device = this.devices[id];
			const voice = device.voices[index];
			detach(voice);
			voice.loop = loop !== 0;
			voice.offset = 0;
			voice.ended = false;
			voice.active = true;
			start(device, voice);
		},

		stop(id, index) {
			const voice = this.devices[id].voices[index];
			detach(voice);
			voice.offset = 0;
			voice.active = false;
		},

		exit_loop(id, index) {
			const device = this.devices[id];
			const voice = device.voices[index];
			if (voice.source) {
				voice.offset = position(device, voice);
				voice.started_at = device.context.currentTime;
				voice.source.loop = false;
			}
			voice.loop = false;
		},

		pause(id, index) {
			const device = this.devices[id];
			const voice = device.voices[index];
			voice.offset = position(device, voice);
			detach(voice);
			voice.active = false;
		},

		resume(id, index) {
			const device = this.devices[id];
			const voice = device.voices[index];
			voice.active = true;
			start(device, voice);
		},

		set_volume(id, index, volume) {
			const voice = this.devices[id].voices[index];
			voice.volume = volume;
			if (voice.gain) {
				voice.gain.gain.value = volume;
			}
		},

		set_rate(id, index, rate) {
			const device = this.devices[id];
			const voice = device.voices[index];
			if (voice.source) {
				voice.offset = position(device, voice);
				voice.started_at = device.context.currentTime;
				voice.source.playbackRate.value = rate;
			}
			voice.rate = rate;
		},

		set_pan(id, index, pan) {
			const voice = this.devices[id].voices[index];
			voice.pan = pan;
			if (voice.panner) {
				voice.panner.pan.value = pan;
			}
		},

		ended(id, index) {
			const voice = this.devices[id].voices[index];
			const was = voice.ended;
			voice.ended = false;
			return was ? 1 : 0;
		},

		suspend(id) {
			const device = this.devices[id];
			device.suspended = true;
			if (device.context) {
				device.context.suspend();
			}
		},

		resume_device(id) {
			const device = this.devices[id];
			device.suspended = false;
			if (device.context) {
				device.context.resume();
			}
		},
	};
});

EM_JS(int, labrador_audio_open, (), {
	return Module.labradorAudio.open();
});

EM_JS(void, labrador_audio_close, (int device), {
	Module.labradorAudio.close(device);
});

EM_JS(int, labrador_audio_add_bank, (int device), {
	return Module.labradorAudio.add_bank(device);
});

EM_JS(int, labrador_audio_add_wave, (int device, int bank, int channels,
	int rate, int frames, const short* samples, int loop_start,
	int loop_length), {
	return Module.labradorAudio.add_wave(device, bank, channels, rate, frames,
		samples, loop_start, loop_length);
});

EM_JS(void, labrador_audio_play_wave, (int device, int bank, int wave,
	double volume, double rate, double pan), {
	Module.labradorAudio.play_wave(device, bank, wave, volume, rate, pan);
});

EM_JS(int, labrador_audio_create_voice, (int device, int bank, int wave), {
	return Module.labradorAudio.create_voice(device, bank, wave);
});

EM_JS(void, labrador_audio_voice_play, (int device, int voice, int loop), {
	Module.labradorAudio.play(device, voice, loop);
});

EM_JS(void, labrador_audio_voice_stop, (int device, int voice), {
	Module.labradorAudio.stop(device, voice);
});

EM_JS(void, labrador_audio_voice_exit_loop, (int device, int voice), {
	Module.labradorAudio.exit_loop(device, voice);
});

EM_JS(void, labrador_audio_voice_pause, (int device, int voice), {
	Module.labradorAudio.pause(device, voice);
});

EM_JS(void, labrador_audio_voice_resume, (int device, int voice), {
	Module.labradorAudio.resume(device, voice);
});

EM_JS(void, labrador_audio_voice_volume, (int device, int voice,
	double volume), {
	Module.labradorAudio.set_volume(device, voice, volume);
});

EM_JS(void, labrador_audio_voice_rate, (int device, int voice, double rate), {
	Module.labradorAudio.set_rate(device, voice, rate);
});

EM_JS(void, labrador_audio_voice_pan, (int device, int voice, double pan), {
	Module.labradorAudio.set_pan(device, voice, pan);
});

EM_JS(int, labrador_audio_voice_ended, (int device, int voice), {
	return Module.labradorAudio.ended(device, voice);
});

EM_JS(void, labrador_audio_suspend, (int device), {
	Module.labradorAudio.suspend(device);
});

EM_JS(void, labrador_audio_resume, (int device), {
	Module.labradorAudio.resume_device(device);
});

namespace labrador
{
	namespace
	{
		// The one place the container's spelling appears, as in xaudio2/.
		std::string container_path(const std::string& directory,
			const std::string& name)
		{
			return directory + name + ".xwb";
		}

		// DirectXTK's mapping, so a pitch sounds the same on both backends:
		// XAudio2SemitonesToFrequencyRatio(pitch * 12), which is 2^pitch.
		double playback_rate(float pitch)
		{
			return std::exp2(static_cast<double>(pitch));
		}

		// One voice's state as the seam reports it. The JavaScript side holds
		// the nodes and the position; this side holds the three-valued state
		// and the loop flag, with DirectXTK's transitions (SoundCommon.h), so
		// that what a game is told does not depend on a promise settling.
		struct WebVoice
		{
			AudioDevice::WaveBankHandle bank;
			int wave = -1;
			SoundState state = SoundState::stopped;
			bool looping = false;
		};
	}

	struct AudioDevice::Impl
	{
		Impl()
		{
			labrador_audio_install();
			this->device = labrador_audio_open();
		}

		~Impl()
		{
			labrador_audio_close(this->device);
		}

		Impl(const Impl&) = delete;
		Impl& operator=(const Impl&) = delete;

		// This device's index in Module.labradorAudio's table.
		int device = -1;

		// Each bank's wave names, in the file's order, which is the index
		// wave_index answers with and the JavaScript side's index too.
		std::vector<std::vector<std::string>> banks;
		std::vector<WebVoice> voices;

		const std::vector<std::string>& bank(WaveBankHandle handle) const
		{
			const size_t index = static_cast<size_t>(handle.index());
			if (!handle.valid() || index >= this->banks.size())
			{
				throw std::out_of_range(
					"A wave bank was reached through an unresolved handle.");
			}
			return this->banks[index];
		}

		// The same words the other two backends use for the same mistake.
		WebVoice& voice(VoiceHandle handle)
		{
			const size_t index = static_cast<size_t>(handle.index());
			if (!handle.valid() || index >= this->voices.size())
			{
				throw std::out_of_range(
					"A voice was reached through an unresolved handle.");
			}
			return this->voices[index];
		}

		// DirectXTK's auto-stop: a playing voice whose wave has run out
		// answers stopped from the next question anybody asks about it.
		WebVoice& settled(VoiceHandle handle)
		{
			WebVoice& state = this->voice(handle);
			if (state.state == SoundState::playing &&
				labrador_audio_voice_ended(this->device, handle.index()) != 0)
			{
				state.state = SoundState::stopped;
			}
			return state;
		}
	};

	AudioDevice::AudioDevice() : impl_(std::make_unique<Impl>())
	{

	}

	AudioDevice::~AudioDevice() = default;
	AudioDevice::AudioDevice(AudioDevice&&) noexcept = default;
	AudioDevice& AudioDevice::operator=(AudioDevice&&) noexcept = default;

	AudioDevice::WaveBankHandle AudioDevice::open_wave_bank(
		const std::string& directory, const std::string& name,
		const std::vector<std::string>& wave_names)
	{
		const std::string path = container_path(directory, name);

		WaveBankData data;
		try
		{
			data = read_xwb_file(path);
		}
		catch (const std::exception& unread)
		{
			// The seam's two failures are told apart by type, and the reader's
			// "no file" is an out_of_range - which here would read as a content
			// bug and defeat the optional-bank substitution. Anything the
			// reader could not do is the container not being there or not
			// being readable, and the reader's own words say which.
			throw std::runtime_error("Failed to open wave bank: " +
				std::string(unread.what()));
		}

		std::vector<std::string> names;
		names.reserve(data.waves.size());
		for (const WaveData& wave : data.waves)
		{
			names.push_back(wave.name);
		}

		// Every name the definition lists, checked at load and in the same
		// words as xaudio2/, for the reason that file gives.
		for (const std::string& wave_name : wave_names)
		{
			if (std::find(names.begin(), names.end(), wave_name) == names.end())
			{
				throw std::out_of_range("Wave bank '" + path +
					"' has no wave named '" + wave_name + "'.");
			}
		}

		// The rate is the one property of a wave the browser judges rather than
		// the reader: the spec promises 8000 to 96000 Hz and browsers take more,
		// each a different range, so the AudioBuffer constructor is asked and
		// its refusal comes back here, where it can name the bank and the wave.
		// The script has already dropped the bank, so the tables still agree.
		const int bank = labrador_audio_add_bank(this->impl_->device);
		for (const WaveData& wave : data.waves)
		{
			if (labrador_audio_add_wave(this->impl_->device, bank,
				wave.channels, wave.sample_rate,
				static_cast<int>(wave.frames()), wave.samples.data(),
				static_cast<int>(wave.loop_start),
				static_cast<int>(wave.loop_length)) == 0)
			{
				throw std::runtime_error("Failed to open wave bank: '" + path +
					"' wave '" + wave.name + "' is " +
					std::to_string(wave.sample_rate) +
					" Hz, which this browser's Web Audio cannot hold.");
			}
		}

		this->impl_->banks.push_back(std::move(names));
		return WaveBankHandle(static_cast<int>(this->impl_->banks.size()) - 1);
	}

	int AudioDevice::wave_index(WaveBankHandle bank,
		const std::string& wave_name) const
	{
		const std::vector<std::string>& names = this->impl_->bank(bank);
		const std::vector<std::string>::const_iterator found =
			std::find(names.begin(), names.end(), wave_name);
		return found == names.end()
			? -1
			: static_cast<int>(found - names.begin());
	}

	AudioDevice::VoiceHandle AudioDevice::create_voice(WaveBankHandle bank,
		int wave_index)
	{
		const std::vector<std::string>& names = this->impl_->bank(bank);
		if (wave_index < 0 ||
			static_cast<size_t>(wave_index) >= names.size())
		{
			throw std::out_of_range(
				"A voice was asked for a wave the bank does not hold.");
		}

		const int voice = labrador_audio_create_voice(this->impl_->device,
			bank.index(), wave_index);
		this->impl_->voices.push_back(WebVoice{ bank, wave_index,
			SoundState::stopped, false });
		return VoiceHandle(voice);
	}

	void AudioDevice::play_wave(WaveBankHandle bank, int wave_index,
		float volume, float pitch, float pan) const
	{
		const std::vector<std::string>& names = this->impl_->bank(bank);
		if (wave_index < 0 ||
			static_cast<size_t>(wave_index) >= names.size())
		{
			throw std::out_of_range(
				"A wave was played that the bank does not hold.");
		}

		labrador_audio_play_wave(this->impl_->device, bank.index(), wave_index,
			volume, playback_rate(pitch), pan);
	}

	void AudioDevice::play_voice(VoiceHandle voice, bool loop, float volume,
		float pitch, float pan) const
	{
		WebVoice& state = this->impl_->settled(voice);
		const int device = this->impl_->device;

		// The levels first, as xaudio2/ sets them before Play.
		labrador_audio_voice_volume(device, voice.index(), volume);
		labrador_audio_voice_rate(device, voice.index(), playback_rate(pitch));
		labrador_audio_voice_pan(device, voice.index(), pan);

		// DirectXTK's Play: a paused voice resumes where it was and keeps its
		// loop, a playing one is left alone, and only a stopped one starts
		// from the beginning with the loop it was asked for.
		if (state.state == SoundState::paused)
		{
			labrador_audio_voice_resume(device, voice.index());
			state.state = SoundState::playing;
		}
		else if (state.state == SoundState::stopped)
		{
			labrador_audio_voice_play(device, voice.index(), loop ? 1 : 0);
			state.state = SoundState::playing;
			state.looping = loop;
		}
	}

	void AudioDevice::stop_voice(VoiceHandle voice, bool immediate) const
	{
		WebVoice& state = this->impl_->settled(voice);

		// DirectXTK's three branches in its order; the top of this file says
		// where the third one parts from what XAudio2 does.
		if (immediate)
		{
			labrador_audio_voice_stop(this->impl_->device, voice.index());
			state.state = SoundState::stopped;
		}
		else if (state.looping)
		{
			labrador_audio_voice_exit_loop(this->impl_->device, voice.index());
			state.looping = false;
		}
	}

	void AudioDevice::pause_voice(VoiceHandle voice) const
	{
		WebVoice& state = this->impl_->settled(voice);
		if (state.state == SoundState::playing)
		{
			labrador_audio_voice_pause(this->impl_->device, voice.index());
			state.state = SoundState::paused;
		}
	}

	void AudioDevice::resume_voice(VoiceHandle voice) const
	{
		WebVoice& state = this->impl_->settled(voice);
		if (state.state == SoundState::paused)
		{
			labrador_audio_voice_resume(this->impl_->device, voice.index());
			state.state = SoundState::playing;
		}
	}

	void AudioDevice::set_voice_volume(VoiceHandle voice, float volume) const
	{
		std::ignore = this->impl_->voice(voice);
		labrador_audio_voice_volume(this->impl_->device, voice.index(), volume);
	}

	void AudioDevice::set_voice_pitch(VoiceHandle voice, float pitch) const
	{
		std::ignore = this->impl_->voice(voice);
		labrador_audio_voice_rate(this->impl_->device, voice.index(),
			playback_rate(pitch));
	}

	void AudioDevice::set_voice_pan(VoiceHandle voice, float pan) const
	{
		std::ignore = this->impl_->voice(voice);
		labrador_audio_voice_pan(this->impl_->device, voice.index(), pan);
	}

	SoundState AudioDevice::voice_state(VoiceHandle voice) const
	{
		return this->impl_->settled(voice).state;
	}

	bool AudioDevice::is_voice_looping(VoiceHandle voice) const
	{
		return this->impl_->voice(voice).looping;
	}

	void AudioDevice::update()
	{
		// Nothing to pump: the browser mixes on its own thread, and a voice
		// that has finished is noticed when somebody asks about it.
	}

	void AudioDevice::suspend()
	{
		labrador_audio_suspend(this->impl_->device);
	}

	void AudioDevice::resume()
	{
		labrador_audio_resume(this->impl_->device);
	}
}
