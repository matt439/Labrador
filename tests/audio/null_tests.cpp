#include <doctest/doctest.h>

#include "engine/assets/sound_bank_loader.h"
#include "engine/audio/audio_device.h"
#include "engine/audio/null/recording.h"
#include "engine/audio/sound_bank.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace labrador;

// What an audible bank actually played, on a machine with no sound card.
//
// THIS FILE IS docs/survey/2026-08-26.md 3.4b's PRODUCT, and 3.4a's list is
// what it spends. That list said: eight of SoundBank's thirteen instance
// methods have no observable behaviour anywhere in this repository, and five
// sites of level clamping - engine arithmetic, sitting below the check for
// the platform - have never executed. Neither was a gap in the tests. An
// audible SoundBank could not be CONSTRUCTED here, because the only
// implementation behind the audio module was DirectXTK's and DirectXTK's
// WaveBank needs an .xwb this repository cannot contain.
// PHILOSOPHY.md:632-637 predicts exactly that failure and names the fix - "a
// seam ships with its headless implementation, or it has not shipped" - and
// engine/audio/null/ is that implementation.
//
// COMPILED IN ONE CONFIGURATION, WHICH IS THE SAME RULE A CLIENT LIVES UNDER. A
// backend is chosen at build time, so a test that names one only compiles when
// that one is built: tests/audio/CMakeLists.txt adds this file when
// LABRADOR_AUDIO_BACKEND is null, exactly as tests/render/ adds its own
// null_tests.cpp. The rest of AudioTests is identical in every configuration.
//
// WHAT IT DOES NOT ASSERT. Nothing here is about sound. There is no mixer, no
// sample rate and no clock behind these calls, so a case cannot say that two
// waves overlapped or that one was louder in any physical sense.
// engine/audio/null/recording.h states the one place the model is deliberately
// weaker than XAudio2's - a voice with no clock never finishes on its own - and
// asserting anything that depended on that would be asserting against this
// backend rather than against the seam.

namespace
{
	// A bank with three waves and two named effects, built the way the loader
	// builds one: open the container, create a voice per effect, hand both to
	// SoundBank. Nothing here reads a file, because a definition is a struct
	// before it is JSON and tests/assets/sound_bank_loader_tests.cpp is where
	// the parse is pinned.
	struct Fixture
	{
		AudioDevice device;
		AudioDevice::WaveBankHandle wave_bank;
		std::unique_ptr<SoundBank> bank;

		Fixture()
		{
			SoundBankDefinition definition;
			definition.source_path = "fixture.json";
			definition.waves = { "shot", "hit", "engine" };
			definition.effects = {
				SoundBankDefinition::Effect{ "engine_loop", "engine" },
				SoundBankDefinition::Effect{ "shot_loop", "shot" }
			};

			this->wave_bank = this->device.open_wave_bank("content/",
				"effects", definition.waves);
			this->bank = build_sound_bank(&this->device, this->wave_bank,
				definition);
		}
	};
}

TEST_CASE("a bank built over a device is audible, and audible is not audible")
{
	const Fixture fixture;

	// audible() is true and this configuration makes no noise whatever, which
	// is the distinction sound_bank.h spells out: is there content, and does
	// this build have an audio API, are two questions and only the first is a
	// property of a bank.
	CHECK(fixture.bank->audible());

	// A bank is the list of names it was handed, on this backend. There is no
	// container to read one out of, so this is what wave_index answers from.
	const std::vector<std::string> expected = { "shot", "hit", "engine" };
	CHECK(recorded_wave_names(fixture.device, fixture.wave_bank) == expected);
}

TEST_CASE("resolving keeps the promise a bank with no content cannot")
{
	const Fixture fixture;

	// The whole T6 guarantee of this class, executed. A misspelt wave throws at
	// load naming the wave; the correct spelling answers with the bank's own
	// index for it. sound_bank_tests.cpp asserts the other half - that a bank
	// with no content accepts both spellings and cannot tell them apart - and
	// the pair of them is the trade sound_bank.h states.
	CHECK(fixture.bank->resolve_wave("hit").index() == 1);
	CHECK_THROWS_AS(std::ignore = fixture.bank->resolve_wave("hti"),
		std::out_of_range);

	CHECK(fixture.bank->resolve_effect("engine_loop").valid());
	CHECK_THROWS_AS(std::ignore = fixture.bank->resolve_effect("engine_lop"),
		std::out_of_range);
}

TEST_CASE("a one-shot reaches the device as the wave the name resolved to")
{
	const Fixture fixture;
	const SoundBank::WaveHandle wave = fixture.bank->resolve_wave("engine");

	fixture.bank->play_wave(wave, 0.5f, 0.25f, -0.75f);

	const std::vector<RecordedSound>& played =
		recorded_sounds(fixture.device);
	REQUIRE(played.size() == 1);
	CHECK(played[0].call == SoundCall::play_wave);
	CHECK(played[0].bank == fixture.wave_bank);
	CHECK(played[0].wave == 2);
	CHECK(played[0].volume == doctest::Approx(0.5f));
	CHECK(played[0].pitch == doctest::Approx(0.25f));
	CHECK(played[0].pan == doctest::Approx(-0.75f));
}

TEST_CASE("the level clamp is engine arithmetic, and this is where it is read")
{
	const Fixture fixture;
	const SoundBank::WaveHandle wave = fixture.bank->resolve_wave("shot");
	const SoundBank::EffectHandle effect =
		fixture.bank->resolve_effect("engine_loop");

	// FIVE SITES, AND THIS IS WHERE THEY ARE ASSERTED - the case
	// docs/survey/2026-08-26.md 3.4a asked for. The clamp sits ABOVE the check
	// for the platform, because folding a volume into [0,1] and a pitch and a
	// pan into [-1,1] is arithmetic engine/audio/ owns - the same kind of
	// engine-side decision as the glyph walk in render/font.h. Below that
	// check, no bank that could be built here would ever reach it and none of
	// the five would execute.
	fixture.bank->play_wave(wave, 500.0f, -12.0f, 9.0f);
	fixture.bank->play_effect(effect, false, -1.0f, 40.0f, -40.0f);
	fixture.bank->set_effect_volume(effect, 1000.0f);
	fixture.bank->set_effect_pitch(effect, -1000.0f);
	fixture.bank->set_effect_pan(effect, 1000.0f);

	const std::vector<RecordedSound>& played =
		recorded_sounds(fixture.device);
	REQUIRE(played.size() == 5);

	CHECK(played[0].volume == doctest::Approx(1.0f));
	CHECK(played[0].pitch == doctest::Approx(-1.0f));
	CHECK(played[0].pan == doctest::Approx(1.0f));

	CHECK(played[1].volume == doctest::Approx(0.0f));
	CHECK(played[1].pitch == doctest::Approx(1.0f));
	CHECK(played[1].pan == doctest::Approx(-1.0f));

	CHECK(played[2].volume == doctest::Approx(1.0f));
	CHECK(played[3].pitch == doctest::Approx(-1.0f));
	CHECK(played[4].pan == doctest::Approx(1.0f));
}

TEST_CASE("all eight of the verbs that answered nothing now say what they did")
{
	const Fixture fixture;
	const SoundBank::WaveHandle wave = fixture.bank->resolve_wave("shot");
	const SoundBank::EffectHandle effect =
		fixture.bank->resolve_effect("engine_loop");

	fixture.bank->play_wave(wave);
	fixture.bank->play_effect(effect, true, 0.5f, 0.1f, 0.2f);
	fixture.bank->stop_effect(effect);
	fixture.bank->stop_effect(effect, true);
	fixture.bank->pause_effect(effect);
	fixture.bank->resume_effect(effect);
	fixture.bank->set_effect_volume(effect, 0.25f);
	fixture.bank->set_effect_pitch(effect, 0.5f);
	fixture.bank->set_effect_pan(effect, 0.75f);

	// The list from 3.4a, in order, with an answer beside each one instead of
	// "it did not throw". That sentence was the finding; this is the sentence
	// that replaces it.
	const std::vector<RecordedSound>& played =
		recorded_sounds(fixture.device);
	REQUIRE(played.size() == 9);
	CHECK(played[0].call == SoundCall::play_wave);
	CHECK(played[1].call == SoundCall::play_voice);
	CHECK(played[2].call == SoundCall::stop_voice);
	CHECK(played[3].call == SoundCall::stop_voice);
	CHECK(played[4].call == SoundCall::pause_voice);
	CHECK(played[5].call == SoundCall::resume_voice);
	CHECK(played[6].call == SoundCall::set_voice_volume);
	CHECK(played[7].call == SoundCall::set_voice_pitch);
	CHECK(played[8].call == SoundCall::set_voice_pan);

	// stop_effect's `immediate` is readable here, which is what makes it more
	// than provably inert: both spellings arrive, and they are different calls
	// rather than the same one twice.
	CHECK(played[2].immediate == false);
	CHECK(played[3].immediate == true);

	// And every one of them names the voice the effect resolved to, which is
	// what makes the forwarding checkable at all: an effect handle is an index
	// into the bank's own table and a voice handle is an index into the
	// device's, and nothing before this could tell you they lined up.
	CHECK(played[1].voice == played[8].voice);
	CHECK(played[1].wave == 2);
}

TEST_CASE("an effect is a voice that persists, and the state says so")
{
	const Fixture fixture;
	const SoundBank::EffectHandle effect =
		fixture.bank->resolve_effect("engine_loop");

	CHECK(fixture.bank->effect_state(effect) == SoundState::stopped);
	CHECK(fixture.bank->is_effect_looping(effect) == false);

	fixture.bank->play_effect(effect, true);
	CHECK(fixture.bank->effect_state(effect) == SoundState::playing);
	CHECK(fixture.bank->is_effect_looping(effect));

	fixture.bank->pause_effect(effect);
	CHECK(fixture.bank->effect_state(effect) == SoundState::paused);

	fixture.bank->resume_effect(effect);
	CHECK(fixture.bank->effect_state(effect) == SoundState::playing);

	// NOT immediate, AND THE ANSWER IS DELIBERATELY THE SURPRISING ONE. Asking
	// a looping voice to stop without insisting leaves the current pass playing
	// and clears the loop, which is DirectXTK's SoundCommon.h transcribed
	// rather than a simplification - engine/audio/null/audio_device.cpp says so
	// where it does it. A model that tidied this up would disagree with the
	// backend it stands in for, which is worse than having no model.
	fixture.bank->stop_effect(effect);
	CHECK(fixture.bank->effect_state(effect) == SoundState::playing);
	CHECK(fixture.bank->is_effect_looping(effect) == false);

	fixture.bank->stop_effect(effect, true);
	CHECK(fixture.bank->effect_state(effect) == SoundState::stopped);
}

TEST_CASE("two effects over one bank are two voices")
{
	const Fixture fixture;
	const SoundBank::EffectHandle engine =
		fixture.bank->resolve_effect("engine_loop");
	const SoundBank::EffectHandle shot =
		fixture.bank->resolve_effect("shot_loop");

	fixture.bank->play_effect(engine, true);

	// The one thing sound_bank_object_tests.cpp said it could not show with two
	// silent banks: handles from different names are different things. Playing
	// one leaves the other alone, which is only assertable where a voice has a
	// state to be left alone in.
	CHECK(fixture.bank->effect_state(engine) == SoundState::playing);
	CHECK(fixture.bank->effect_state(shot) == SoundState::stopped);
}

TEST_CASE("a definition may name a wave the wave list did not")
{
	AudioDevice device;

	SoundBankDefinition definition;
	definition.source_path = "fixture.json";
	definition.waves = { "shot" };
	definition.effects = {
		SoundBankDefinition::Effect{ "engine_loop", "engine" }
	};

	// The loader adds an effect's wave to the list when the `waves` array did
	// not carry it, and this is what would break if it stopped: the seam is
	// asked for a wave nothing declared, answers -1, and build_sound_bank
	// refuses naming the definition and the wave. The parse that keeps this
	// from happening is pinned in tests/assets/sound_bank_loader_tests.cpp; the
	// throw underneath it is pinned here, because it needs a device.
	const AudioDevice::WaveBankHandle wave_bank =
		device.open_wave_bank("content/", "effects", definition.waves);
	CHECK_THROWS_AS(std::ignore = build_sound_bank(&device, wave_bank,
		definition), std::out_of_range);
}
