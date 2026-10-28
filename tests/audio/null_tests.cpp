#include <doctest/doctest.h>

#include "engine/assets/sound_bank_loader.h"
#include "engine/audio/audio_device.h"
#include "engine/audio/audio_resources.h"
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
// implementation behind the audio module was DirectXTK's, and DirectXTK's
// WaveBank needs an .xwb and an audio device.
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
	// device's, and this is what tells you they line up.
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

	// What two silent banks cannot show: handles from different names are different things. Playing
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

namespace
{
	// Two banks over one device, both added to one AudioResources, with a
	// music group and an effects group. Each bank has a wave and two effects;
	// nothing is assigned to a group until a case does it.
	struct GroupFixture
	{
		AudioDevice device;
		AudioResources resources;
		SoundBank* first = nullptr;
		SoundBank* second = nullptr;
		AudioResources::SoundGroupHandle music;
		AudioResources::SoundGroupHandle effects;

		GroupFixture()
		{
			SoundBankDefinition definition;
			definition.source_path = "fixture.json";
			definition.waves = { "shot", "theme" };
			definition.effects = {
				SoundBankDefinition::Effect{ "theme_loop", "theme" },
				SoundBankDefinition::Effect{ "shot_loop", "shot" }
			};

			const AudioDevice::WaveBankHandle first_waves =
				this->device.open_wave_bank("content/", "first",
					definition.waves);
			const AudioDevice::WaveBankHandle second_waves =
				this->device.open_wave_bank("content/", "second",
					definition.waves);
			this->resources.add_sound_bank("first", build_sound_bank(
				&this->device, first_waves, definition));
			this->resources.add_sound_bank("second", build_sound_bank(
				&this->device, second_waves, definition));
			this->first = this->resources.sound_bank("first");
			this->second = this->resources.sound_bank("second");

			this->music = this->resources.add_sound_group("music");
			this->effects = this->resources.add_sound_group("effects");
		}

		const std::vector<RecordedSound>& played() const
		{
			return recorded_sounds(this->device);
		}
	};
}

TEST_CASE("a grouped one-shot launches at its level times its group's")
{
	GroupFixture fixture;
	const SoundBank::WaveHandle shot = fixture.first->resolve_wave("shot");
	const SoundBank::WaveHandle theme = fixture.first->resolve_wave("theme");
	fixture.first->set_wave_group(shot, fixture.effects);
	fixture.resources.set_group_volume(fixture.effects, 0.5f);

	fixture.first->play_wave(shot, 0.5f);
	fixture.first->play_wave(theme, 0.5f);

	// Clamped before the multiply, so a level above 1 is 1 times the group.
	fixture.first->play_wave(shot, 3.0f);

	const std::vector<RecordedSound>& played = fixture.played();
	REQUIRE(played.size() == 3);
	CHECK(played[0].volume == doctest::Approx(0.25f));
	CHECK(played[1].volume == doctest::Approx(0.5f));
	CHECK(played[2].volume == doctest::Approx(0.5f));
}

TEST_CASE("a group change reaches the next one-shot and not one in flight")
{
	GroupFixture fixture;
	const SoundBank::WaveHandle shot = fixture.first->resolve_wave("shot");
	fixture.first->set_wave_group(shot, fixture.effects);

	fixture.first->play_wave(shot, 0.8f);
	fixture.resources.set_group_volume(fixture.effects, 0.5f);
	fixture.first->play_wave(shot, 0.8f);

	// A one-shot has no voice to push a level to, so the group change records
	// nothing between the two plays.
	const std::vector<RecordedSound>& played = fixture.played();
	REQUIRE(played.size() == 2);
	CHECK(played[0].call == SoundCall::play_wave);
	CHECK(played[0].volume == doctest::Approx(0.8f));
	CHECK(played[1].call == SoundCall::play_wave);
	CHECK(played[1].volume == doctest::Approx(0.4f));
}

TEST_CASE("a group change pushes to its playing and paused effects in every bank")
{
	GroupFixture fixture;
	const SoundBank::EffectHandle theme =
		fixture.first->resolve_effect("theme_loop");
	const SoundBank::EffectHandle shot =
		fixture.first->resolve_effect("shot_loop");
	const SoundBank::EffectHandle second_theme =
		fixture.second->resolve_effect("theme_loop");
	const SoundBank::EffectHandle second_shot =
		fixture.second->resolve_effect("shot_loop");

	// Assigned out of handle order, to show the walk is in handle order.
	fixture.first->set_effect_group(shot, fixture.music);
	fixture.first->set_effect_group(theme, fixture.music);
	fixture.second->set_effect_group(second_theme, fixture.music);
	fixture.second->set_effect_group(second_shot, fixture.effects);

	fixture.first->play_effect(theme, true, 0.8f);
	fixture.first->play_effect(shot, true, 0.6f);
	fixture.second->play_effect(second_theme, true, 0.4f);
	fixture.second->play_effect(second_shot, true, 1.0f);
	fixture.second->pause_effect(second_theme);

	const size_t before = fixture.played().size();
	fixture.resources.set_group_volume(fixture.music, 0.5f);

	// Bank then effect: the first bank's two, then the second bank's paused
	// one, and nothing for the effects group.
	const std::vector<RecordedSound>& played = fixture.played();
	REQUIRE(played.size() == before + 3);
	CHECK(played[before].call == SoundCall::set_voice_volume);
	CHECK(played[before].voice == played[0].voice);
	CHECK(played[before].volume == doctest::Approx(0.4f));
	CHECK(played[before + 1].call == SoundCall::set_voice_volume);
	CHECK(played[before + 1].voice == played[1].voice);
	CHECK(played[before + 1].volume == doctest::Approx(0.3f));
	CHECK(played[before + 2].call == SoundCall::set_voice_volume);
	CHECK(played[before + 2].voice == played[2].voice);
	CHECK(played[before + 2].volume == doctest::Approx(0.2f));

	// The paused effect took the level now, and resumes at it.
	CHECK(fixture.second->effect_state(second_theme) == SoundState::paused);
}

TEST_CASE("empty and refilled banks preserve group notification order")
{
	GroupFixture fixture;
	const AudioResources::SoundBankHandle first =
		fixture.resources.resolve_sound_bank("first");
	const SoundBank::EffectHandle second_theme =
		fixture.second->resolve_effect("theme_loop");
	fixture.second->set_effect_group(second_theme, fixture.music);
	const size_t second_play = fixture.played().size();
	fixture.second->play_effect(second_theme, true, 0.4f);

	fixture.resources.add_sound_bank("first", nullptr);
	fixture.first = nullptr;
	fixture.resources.add_sound_bank("empty", nullptr);
	const AudioResources::SoundBankHandle empty =
		fixture.resources.resolve_sound_bank("empty");
	CHECK(fixture.resources.resolve_sound_bank("first") == first);
	CHECK_THROWS_AS(std::ignore = fixture.resources.sound_bank(first),
		std::out_of_range);
	CHECK_THROWS_AS(std::ignore = fixture.resources.sound_bank(empty),
		std::out_of_range);

	const size_t before_empty = fixture.played().size();
	fixture.resources.set_group_volume(fixture.music, 0.25f);
	const std::vector<RecordedSound>& played = fixture.played();
	REQUIRE(played.size() == before_empty + 1);
	CHECK(played[before_empty].call == SoundCall::set_voice_volume);
	CHECK(played[before_empty].voice == played[second_play].voice);
	CHECK(played[before_empty].volume == doctest::Approx(0.1f));

	SoundBankDefinition definition;
	definition.source_path = "replacement.json";
	definition.waves = { "theme" };
	definition.effects = {
		SoundBankDefinition::Effect{ "theme_loop", "theme" }
	};
	const AudioDevice::WaveBankHandle waves = fixture.device.open_wave_bank(
		"content/", "replacement", definition.waves);
	fixture.resources.add_sound_bank("first", build_sound_bank(
		&fixture.device, waves, definition));
	fixture.first = fixture.resources.sound_bank(first);
	CHECK(fixture.resources.resolve_sound_bank("first") == first);
	const SoundBank::EffectHandle first_theme =
		fixture.first->resolve_effect("theme_loop");
	fixture.first->set_effect_group(first_theme, fixture.music);
	const size_t first_play = fixture.played().size();
	fixture.first->play_effect(first_theme, true, 0.8f);

	const size_t before_refill = played.size();
	fixture.resources.set_group_volume(fixture.music, 0.5f);
	REQUIRE(played.size() == before_refill + 2);
	CHECK(played[before_refill].call == SoundCall::set_voice_volume);
	CHECK(played[before_refill].voice == played[first_play].voice);
	CHECK(played[before_refill].volume == doctest::Approx(0.4f));
	CHECK(played[before_refill + 1].call == SoundCall::set_voice_volume);
	CHECK(played[before_refill + 1].voice == played[second_play].voice);
	CHECK(played[before_refill + 1].volume == doctest::Approx(0.2f));
	CHECK(fixture.resources.resolve_sound_bank("empty") == empty);
	CHECK_THROWS_AS(std::ignore = fixture.resources.sound_bank(empty),
		std::out_of_range);
}

TEST_CASE("a stopped effect takes its group's volume at its next play")
{
	GroupFixture fixture;
	const SoundBank::EffectHandle theme =
		fixture.first->resolve_effect("theme_loop");
	fixture.first->set_effect_group(theme, fixture.music);

	fixture.resources.set_group_volume(fixture.music, 0.5f);
	CHECK(fixture.played().empty());

	fixture.first->play_effect(theme, true, 0.6f);
	fixture.first->set_effect_volume(theme, 0.2f);
	fixture.resources.set_group_volume(fixture.music, 0.25f);

	// Both of the caller's levels went out multiplied, and the group change
	// pushed the last one the caller asked for, not the product.
	const std::vector<RecordedSound>& played = fixture.played();
	REQUIRE(played.size() == 3);
	CHECK(played[0].call == SoundCall::play_voice);
	CHECK(played[0].volume == doctest::Approx(0.3f));
	CHECK(played[1].call == SoundCall::set_voice_volume);
	CHECK(played[1].volume == doctest::Approx(0.1f));
	CHECK(played[2].call == SoundCall::set_voice_volume);
	CHECK(played[2].volume == doctest::Approx(0.05f));
}

TEST_CASE("a sound in no group is at the level it asked for")
{
	GroupFixture fixture;
	const SoundBank::WaveHandle shot = fixture.first->resolve_wave("shot");
	const SoundBank::EffectHandle theme =
		fixture.first->resolve_effect("theme_loop");

	fixture.resources.set_group_volume(fixture.music, 0.0f);
	fixture.resources.set_group_volume(fixture.effects, 0.0f);
	fixture.first->play_wave(shot, 0.7f);
	fixture.first->play_effect(theme, true, 0.6f);

	const std::vector<RecordedSound>& played = fixture.played();
	REQUIRE(played.size() == 2);
	CHECK(played[0].volume == doctest::Approx(0.7f));
	CHECK(played[1].volume == doctest::Approx(0.6f));
}

TEST_CASE("an audible bank refuses a group handle above everything else")
{
	GroupFixture fixture;
	const SoundBank::WaveHandle shot = fixture.first->resolve_wave("shot");
	const SoundBank::EffectHandle theme =
		fixture.first->resolve_effect("theme_loop");
	const AudioResources::SoundGroupHandle never_resolved;

	CHECK_THROWS_AS(fixture.first->set_wave_group(shot, never_resolved),
		std::out_of_range);
	CHECK_THROWS_AS(fixture.first->set_effect_group(theme, never_resolved),
		std::out_of_range);
	CHECK_THROWS_AS(fixture.first->set_effect_group(
		SoundBank::EffectHandle(9), fixture.music), std::out_of_range);
	CHECK(fixture.played().empty());
}
