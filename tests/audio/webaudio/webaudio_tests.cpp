#include <doctest/doctest.h>

#include "engine/assets/sound_bank_loader.h"
#include "engine/audio/audio_device.h"
#include "engine/audio/audio_resources.h"
#include "engine/audio/sound_bank.h"

#include <emscripten/em_js.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace labrador;

// The Web Audio backend where there is no Web Audio, which is Node.
//
// COMPILED WHEN THE BACKEND IS webaudio, and that is a browser build, whose
// tests run under Node. There the device has no AudioContext and never will,
// so these cases are everything engine/audio/webaudio/ answers without a
// sound: the container read through the engine's reader, the two throws the
// seam tells apart by type, and DirectXTK's state transitions, which this
// backend keeps on the C++ side so that what a game is told does not wait on
// the browser. What reaches a speaker needs a page and a person;
// samples/audio/README.md owns playback.
//
// The bank is the audio sample's tones.xwb, which this folder's CMakeLists
// copies beside the executable for xwb_file_tests.cpp.
//
// THE LAST CASES GIVE THE BACKEND A WEB AUDIO THAT RECORDS, because what it
// does with a wave's loop region happens only once a context exists: the
// region goes to the source node as seconds, and a paused voice's position is
// folded back into it. The stand-in below is constructors that keep what they
// were handed and a clock the case moves by hand. It holds this backend's
// arithmetic and the values it passes; it says nothing about what a browser
// plays from them. Their bank is content/loop_region.xwb, which
// content/make_loop_bank.py writes and describes.

// Stands a recording Web Audio and a window in for the ones Node lacks, and
// sets the backend's script object aside so that the next device installs a
// new one that sees them. Sources are kept in the order they were created.
EM_JS(void, test_install_fake_web_audio, (), {
	const fake = {
		saved_audio: Module.labradorAudio,
		saved_window: Object.getOwnPropertyDescriptor(globalThis, "window"),
		now: 0,
		sources: [],
		listeners: [],
	};
	class FakeNode {
		connect() {
		}
		disconnect() {
		}
	}
	globalThis.AudioContext = class {
		constructor() {
			this.state = "running";
			this.destination = new FakeNode();
		}
		get currentTime() {
			return fake.now;
		}
		suspend() {
			this.state = "suspended";
		}
		resume() {
			this.state = "running";
		}
		close() {
			this.state = "closed";
		}
	};
	globalThis.AudioBuffer = class {
		constructor(options) {
			this.channels = [];
			for (let channel = 0; channel < options.numberOfChannels; channel++) {
				this.channels.push(new Float32Array(options.length));
			}
		}
		getChannelData(channel) {
			return this.channels[channel];
		}
	};
	globalThis.AudioBufferSourceNode = class extends FakeNode {
		constructor(context, options) {
			super();
			this.loop = options.loop;
			this.loopStart = options.loopStart;
			this.loopEnd = options.loopEnd;
			this.playbackRate = { value: options.playbackRate };
			this.onended = null;
			this.offset = -1;
			fake.sources.push(this);
		}
		start(when, offset) {
			this.offset = offset === undefined ? 0 : offset;
		}
		stop() {
		}
	};
	globalThis.GainNode = class extends FakeNode {
		constructor(context, options) {
			super();
			this.gain = { value: options.gain };
		}
	};
	globalThis.StereoPannerNode = class extends FakeNode {
		constructor(context, options) {
			super();
			this.pan = { value: options.pan };
		}
	};
	Object.defineProperty(globalThis, "window", {
		value: {
			addEventListener(name, listener, capture) {
				fake.listeners.push({
					name: name,
					listener: listener,
					capture: capture,
				});
			},
			removeEventListener(name, listener) {
				fake.listeners = fake.listeners.filter((entry) =>
					!(entry.name === name && entry.listener === listener));
			},
		},
		configurable: true,
		writable: true,
	});
	Module.labradorAudio = undefined;
	globalThis.labradorFakeAudio = fake;
});

// Puts back what the install replaced. Every device made in between must be
// gone by now, because it is an index into the script object dropped here.
EM_JS(void, test_remove_fake_web_audio, (), {
	const fake = globalThis.labradorFakeAudio;
	Module.labradorAudio = fake.saved_audio;
	for (const name of ["AudioContext", "AudioBuffer", "AudioBufferSourceNode",
		"GainNode", "StereoPannerNode"]) {
		delete globalThis[name];
	}
	if (fake.saved_window) {
		Object.defineProperty(globalThis, "window", fake.saved_window);
	} else {
		delete globalThis.window;
	}
	delete globalThis.labradorFakeAudio;
});

// A keydown at the fake window: Escape, which grants no activation, or a
// letter, which does.
EM_JS(void, test_send_key, (int escape), {
	const event = { type: "keydown", key: escape ? "Escape" : "a" };
	for (const entry of globalThis.labradorFakeAudio.listeners.slice()) {
		if (entry.name === event.type) {
			entry.listener(event);
		}
	}
});

// Moves the fake context's clock on.
EM_JS(void, test_advance_clock, (double seconds), {
	globalThis.labradorFakeAudio.now += seconds;
});

EM_JS(int, test_source_count, (), {
	return globalThis.labradorFakeAudio.sources.length;
});

// One of a source node's values: 0 loop as 0 or 1, 1 loopStart, 2 loopEnd,
// 3 the offset it was started at, 4 its playback rate.
EM_JS(double, test_source_value, (int source, int which), {
	const node = globalThis.labradorFakeAudio.sources[source];
	return [node.loop ? 1 : 0, node.loopStart, node.loopEnd, node.offset,
		node.playbackRate.value][which];
});

namespace
{
	const std::vector<std::string> tone_names = { "ping", "chime", "drone" };

	// loop_region.xwb, at 8,000 Hz: `looped` is 4,000 frames looping frames
	// 1,000 to 3,000, and `whole` is 2,000 frames with no region.
	const std::vector<std::string> loop_names = { "looped", "whole" };

	constexpr int source_loop = 0;
	constexpr int source_loop_start = 1;
	constexpr int source_loop_end = 2;
	constexpr int source_offset = 3;
	constexpr int source_rate = 4;

	// The recording Web Audio for one case's length, so that every other
	// case in the suite still runs where there is none. Declare it before the
	// AudioDevice it serves: the device has to be destroyed first.
	struct FakeWebAudio
	{
		FakeWebAudio()
		{
			test_install_fake_web_audio();
		}

		~FakeWebAudio()
		{
			test_remove_fake_web_audio();
		}

		FakeWebAudio(const FakeWebAudio&) = delete;
		FakeWebAudio& operator=(const FakeWebAudio&) = delete;
	};

	struct Fixture
	{
		AudioDevice device;
		AudioDevice::WaveBankHandle wave_bank;
		std::unique_ptr<SoundBank> bank;

		Fixture()
		{
			SoundBankDefinition definition;
			definition.source_path = "tones.json";
			definition.waves = tone_names;
			definition.effects = {
				SoundBankDefinition::Effect{ "drone_loop", "drone" },
				SoundBankDefinition::Effect{ "ping_once", "ping" }
			};

			this->wave_bank = this->device.open_wave_bank("./content/",
				"tones", definition.waves);
			this->bank = build_sound_bank(&this->device, this->wave_bank,
				definition);
		}
	};
}

TEST_CASE("a bank is read from its container, and its order is the file's")
{
	AudioDevice device;

	// Listed out of order on purpose: the index is where the wave sits in
	// the container, not where the definition happened to name it.
	const AudioDevice::WaveBankHandle bank = device.open_wave_bank(
		"./content/", "tones", { "drone", "ping" });

	CHECK(device.wave_index(bank, "ping") == 0);
	CHECK(device.wave_index(bank, "chime") == 1);
	CHECK(device.wave_index(bank, "drone") == 2);
	CHECK(device.wave_index(bank, "thud") == -1);
}

TEST_CASE("a missing container and a missing wave are different throws")
{
	AudioDevice device;

	// runtime_error, which is what ResourceLoader catches to substitute a
	// silent bank for an optional one - even though the engine's reader says
	// "no file" with an out_of_range, which this backend must not pass on.
	CHECK_THROWS_AS(device.open_wave_bank("./content/", "absent", tone_names),
		std::runtime_error);
	try
	{
		device.open_wave_bank("./content/", "absent", tone_names);
	}
	catch (const std::out_of_range&)
	{
		FAIL("a missing container read as a content bug");
	}
	catch (const std::runtime_error& error)
	{
		CHECK(std::string(error.what()).find("absent.xwb") !=
			std::string::npos);
	}

	// out_of_range, the content bug: the container is there and does not
	// hold a wave the definition names.
	CHECK_THROWS_AS(device.open_wave_bank("./content/", "tones",
		{ "ping", "thud" }), std::out_of_range);
}

TEST_CASE("a wave or a voice the bank does not hold is refused")
{
	AudioDevice device;
	const AudioDevice::WaveBankHandle bank = device.open_wave_bank(
		"./content/", "tones", tone_names);

	CHECK_THROWS_AS(device.create_voice(bank, 3), std::out_of_range);
	CHECK_THROWS_AS(device.create_voice(bank, -1), std::out_of_range);
	CHECK_THROWS_AS(device.play_wave(bank, 3, 1.0f, 0.0f, 0.0f),
		std::out_of_range);
	CHECK_THROWS_AS(device.voice_state(AudioDevice::VoiceHandle()),
		std::out_of_range);
	CHECK_THROWS_AS(device.wave_index(AudioDevice::WaveBankHandle(), "ping"),
		std::out_of_range);

	// And one that it does hold plays without a sound to play through.
	CHECK_NOTHROW(device.play_wave(bank, 0, 1.0f, 0.5f, -0.5f));
}

TEST_CASE("a voice moves through DirectXTK's states with no context behind it")
{
	const Fixture fixture;
	const SoundBank::EffectHandle drone =
		fixture.bank->resolve_effect("drone_loop");

	CHECK(fixture.bank->effect_state(drone) == SoundState::stopped);

	fixture.bank->play_effect(drone, true, 0.5f, 0.25f, -0.5f);
	CHECK(fixture.bank->effect_state(drone) == SoundState::playing);
	CHECK(fixture.bank->is_effect_looping(drone));

	fixture.bank->pause_effect(drone);
	CHECK(fixture.bank->effect_state(drone) == SoundState::paused);

	// Pausing a paused voice changes nothing, and neither does resuming a
	// playing one.
	fixture.bank->pause_effect(drone);
	CHECK(fixture.bank->effect_state(drone) == SoundState::paused);

	// Play on a paused voice resumes it and keeps its loop, whatever loop
	// it is asked for, which is DirectXTK's Play.
	fixture.bank->play_effect(drone, false);
	CHECK(fixture.bank->effect_state(drone) == SoundState::playing);
	CHECK(fixture.bank->is_effect_looping(drone));

	fixture.bank->resume_effect(drone);
	CHECK(fixture.bank->effect_state(drone) == SoundState::playing);

	fixture.bank->set_effect_volume(drone, 0.75f);
	fixture.bank->set_effect_pitch(drone, -1.0f);
	fixture.bank->set_effect_pan(drone, 1.0f);
	CHECK(fixture.bank->effect_state(drone) == SoundState::playing);

	// Not immediate on a looping voice: the loop ends and the pass plays on.
	fixture.bank->stop_effect(drone);
	CHECK(fixture.bank->effect_state(drone) == SoundState::playing);
	CHECK_FALSE(fixture.bank->is_effect_looping(drone));

	fixture.bank->stop_effect(drone, true);
	CHECK(fixture.bank->effect_state(drone) == SoundState::stopped);

	// A stopped voice resumes nothing, and plays again from the start with
	// the loop it is now asked for.
	fixture.bank->resume_effect(drone);
	CHECK(fixture.bank->effect_state(drone) == SoundState::stopped);
	fixture.bank->play_effect(drone, false);
	CHECK(fixture.bank->effect_state(drone) == SoundState::playing);
	CHECK_FALSE(fixture.bank->is_effect_looping(drone));
}

TEST_CASE("a playing voice keeps the loop it was started with")
{
	const Fixture fixture;
	const SoundBank::EffectHandle ping =
		fixture.bank->resolve_effect("ping_once");

	fixture.bank->play_effect(ping, false);
	fixture.bank->play_effect(ping, true);
	CHECK(fixture.bank->effect_state(ping) == SoundState::playing);
	CHECK_FALSE(fixture.bank->is_effect_looping(ping));

	// With no clock, a voice that is not looping never reaches its end, so
	// it answers playing until it is stopped - the same answer the null
	// backend gives, for the same reason.
	fixture.bank->stop_effect(ping);
	CHECK(fixture.bank->effect_state(ping) == SoundState::playing);
}

TEST_CASE("group volume calls are accepted and leave each voice's state alone")
{
	// The group arithmetic is SoundBank's, above the seam, so all this
	// backend sees is the product through the verbs it already has - and
	// a paused voice taking a new level is one of them. This device keeps no
	// level a test can read back; null_tests.cpp pins the products.
	AudioDevice device;
	SoundBankDefinition definition;
	definition.source_path = "tones.json";
	definition.waves = tone_names;
	definition.effects = {
		SoundBankDefinition::Effect{ "drone_loop", "drone" }
	};
	const AudioDevice::WaveBankHandle wave_bank = device.open_wave_bank(
		"./content/", "tones", definition.waves);

	AudioResources resources;
	resources.add_sound_bank("tones",
		build_sound_bank(&device, wave_bank, definition));
	SoundBank* bank = resources.sound_bank("tones");
	const AudioResources::SoundGroupHandle music =
		resources.add_sound_group("music");

	const SoundBank::WaveHandle ping = bank->resolve_wave("ping");
	const SoundBank::EffectHandle drone = bank->resolve_effect("drone_loop");
	CHECK_NOTHROW(bank->set_wave_group(ping, music));
	CHECK_NOTHROW(bank->set_effect_group(drone, music));

	CHECK_NOTHROW(resources.set_group_volume(music, 0.5f));
	CHECK_NOTHROW(bank->play_wave(ping, 0.5f));
	CHECK_NOTHROW(bank->play_effect(drone, true, 0.5f));
	CHECK_NOTHROW(resources.set_group_volume(music, 0.25f));
	bank->pause_effect(drone);
	CHECK_NOTHROW(resources.set_group_volume(music, 1.0f));
	CHECK(bank->effect_state(drone) == SoundState::paused);
	bank->resume_effect(drone);
	CHECK(bank->effect_state(drone) == SoundState::playing);
}

TEST_CASE("devices are independent, and survive a move")
{
	AudioDevice first;
	AudioDevice second;
	const AudioDevice::WaveBankHandle bank = first.open_wave_bank(
		"./content/", "tones", tone_names);
	const AudioDevice::VoiceHandle voice = first.create_voice(bank, 2);
	first.play_voice(voice, true, 1.0f, 0.0f, 0.0f);

	// The second device has no banks of its own: the first one's handle
	// indexes nothing there.
	CHECK_THROWS_AS(second.wave_index(bank, "drone"), std::out_of_range);

	AudioDevice moved(std::move(first));
	CHECK(moved.voice_state(voice) == SoundState::playing);
	CHECK(moved.is_voice_looping(voice));

	moved.suspend();
	moved.resume();
	moved.update();
	CHECK(moved.voice_state(voice) == SoundState::playing);
}

TEST_CASE("a wave's loop region reaches its source node, in seconds")
{
	const FakeWebAudio fake;
	AudioDevice device;
	const AudioDevice::WaveBankHandle bank = device.open_wave_bank(
		"./content/", "loop_region", loop_names);
	const AudioDevice::VoiceHandle looped = device.create_voice(bank, 0);
	const AudioDevice::VoiceHandle whole = device.create_voice(bank, 1);
	const AudioDevice::VoiceHandle once = device.create_voice(bank, 0);

	// Asked for before the gesture, so nothing is built yet, and Escape is
	// not the gesture.
	device.play_voice(looped, true, 1.0f, 0.0f, 0.0f);
	device.play_voice(whole, true, 1.0f, 0.0f, 0.0f);
	device.play_voice(once, false, 1.0f, 0.0f, 0.0f);
	CHECK(test_source_count() == 0);
	test_send_key(1);
	CHECK(test_source_count() == 0);

	// The waiting voices start inside the gesture, in the order they were
	// created.
	test_send_key(0);
	REQUIRE(test_source_count() == 3);

	// Frames 1,000 to 3,000 at 8,000 Hz. The voice starts at the wave's
	// beginning, so what comes before the region plays once.
	CHECK(test_source_value(0, source_loop) == 1.0);
	CHECK(test_source_value(0, source_loop_start) == doctest::Approx(0.125));
	CHECK(test_source_value(0, source_loop_end) == doctest::Approx(0.375));
	CHECK(test_source_value(0, source_offset) == doctest::Approx(0.0));

	// No region: both ends are zero, which Web Audio reads as the whole
	// buffer.
	CHECK(test_source_value(1, source_loop) == 1.0);
	CHECK(test_source_value(1, source_loop_start) == 0.0);
	CHECK(test_source_value(1, source_loop_end) == 0.0);

	// The region loops nothing unless the voice was asked to loop, and a
	// one-shot never is.
	CHECK(test_source_value(2, source_loop) == 0.0);
	device.play_wave(bank, 0, 1.0f, 0.0f, 0.0f);
	REQUIRE(test_source_count() == 4);
	CHECK(test_source_value(3, source_loop) == 0.0);
}

TEST_CASE("a paused looping voice resumes from inside its loop region")
{
	const FakeWebAudio fake;
	AudioDevice device;
	const AudioDevice::WaveBankHandle bank = device.open_wave_bank(
		"./content/", "loop_region", loop_names);
	const AudioDevice::VoiceHandle voice = device.create_voice(bank, 0);
	test_send_key(0);

	device.play_voice(voice, true, 1.0f, 0.0f, 0.0f);
	REQUIRE(test_source_count() == 1);

	// Half a second in, the voice has played the 0.125 s before the region,
	// the 0.25 s region once and 0.125 s of it again.
	test_advance_clock(0.5);
	device.pause_voice(voice);
	device.resume_voice(voice);
	REQUIRE(test_source_count() == 2);
	CHECK(test_source_value(1, source_offset) == doctest::Approx(0.25));
	CHECK(test_source_value(1, source_loop) == 1.0);
	CHECK(test_source_value(1, source_loop_start) == doctest::Approx(0.125));
	CHECK(test_source_value(1, source_loop_end) == doctest::Approx(0.375));

	// Short of the region's end nothing is folded.
	test_advance_clock(0.0625);
	device.pause_voice(voice);
	device.resume_voice(voice);
	REQUIRE(test_source_count() == 3);
	CHECK(test_source_value(2, source_offset) == doctest::Approx(0.3125));

	// An octave up the wave runs at twice the clock: 0.0625 s is 0.125 s of
	// wave, which passes the region's end by 0.0625 s.
	device.set_voice_pitch(voice, 1.0f);
	test_advance_clock(0.0625);
	device.pause_voice(voice);
	device.resume_voice(voice);
	REQUIRE(test_source_count() == 4);
	CHECK(test_source_value(3, source_offset) == doctest::Approx(0.1875));
	CHECK(test_source_value(3, source_rate) == doctest::Approx(2.0));

	// A stop that is not immediate leaves the loop: the node stops looping
	// where it is, and the position runs on past the region's end towards
	// the wave's.
	device.stop_voice(voice, false);
	CHECK(test_source_value(3, source_loop) == 0.0);
	CHECK_FALSE(device.is_voice_looping(voice));
	test_advance_clock(0.125);
	device.pause_voice(voice);
	device.resume_voice(voice);
	REQUIRE(test_source_count() == 5);
	CHECK(test_source_value(4, source_offset) == doctest::Approx(0.4375));
	CHECK(test_source_value(4, source_loop) == 0.0);
}

TEST_CASE("a looping voice with no region wraps at the end of its wave")
{
	const FakeWebAudio fake;
	AudioDevice device;
	const AudioDevice::WaveBankHandle bank = device.open_wave_bank(
		"./content/", "loop_region", loop_names);
	const AudioDevice::VoiceHandle voice = device.create_voice(bank, 1);
	test_send_key(0);

	// A quarter-second wave, once round and half of it again.
	device.play_voice(voice, true, 1.0f, 0.0f, 0.0f);
	test_advance_clock(0.375);
	device.pause_voice(voice);
	device.resume_voice(voice);
	REQUIRE(test_source_count() == 2);
	CHECK(test_source_value(1, source_offset) == doctest::Approx(0.125));
	CHECK(test_source_value(1, source_loop_start) == 0.0);
	CHECK(test_source_value(1, source_loop_end) == 0.0);
}
