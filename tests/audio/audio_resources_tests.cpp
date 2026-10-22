#include <doctest/doctest.h>

#include "engine/audio/audio_resources.h"
#include "engine/audio/sound_bank.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>

using namespace labrador;

// The bank table, which needs no device and never did.
//
// AudioResources' bank table is a Registry<SoundBank> and four forwarding methods, so most
// of what is asserted here is the registry contract - and that is the point of
// asserting it. tests/core/registry_tests.cpp pins the template; this pins that
// the audio module gets the same bargain as the render one, which is the claim
// audio_resources.h opens with when it calls itself "the audio counterpart to
// RenderResources".
//
// Every bank below is SoundBank::silent(), and it no longer has to be: since
// the audio seam landed, a bank over engine/audio/null/ can be built and read
// back (tests/audio/null_tests.cpp). It stays that way here because a bank with
// no content needs no device and compiles in all six configurations, and none
// of this file is about what a bank plays - a registry does not know what it
// holds, which is the property being asserted.
//
// The sound groups are the other table here, and the same holds for them:
// what this file asserts is the table - names, handles, the clamp - and the
// one thing a bank with no content does with a group. What a group volume
// does to the levels an audible bank sends is in null_tests.cpp.

TEST_CASE("a bank name is resolved once and read by handle after that")
{
	AudioResources resources;
	resources.add_sound_bank("effects", SoundBank::silent());

	const AudioResources::SoundBankHandle handle =
		resources.resolve_sound_bank("effects");
	CHECK(handle.valid());

	// The bargain the header states: resolve a name once, keep the handle, and
	// never search by name again. Both ways of asking answer with the same
	// object, which is what makes the second way safe to stop using.
	CHECK(resources.sound_bank(handle) == resources.sound_bank("effects"));
	CHECK(resources.sound_bank(handle)->audible() == false);
}

TEST_CASE("resolving a bank nothing loaded throws, and names it")
{
	const AudioResources resources;

	// T6, through Registry's own sentence. The kind is spelt "SoundBank" at
	// the registry's construction in audio_resources.h, and a person reading
	// this at load time needs both halves of the message to act on it: which
	// kind of thing was missing and which name was asked for.
	std::string message = "no error was thrown";
	try
	{
		std::ignore = resources.resolve_sound_bank("music");
	}
	catch (const std::out_of_range& error)
	{
		message = error.what();
	}

	CHECK(message.find("SoundBank") != std::string::npos);
	CHECK(message.find("'music'") != std::string::npos);
}

TEST_CASE("re-adding a name refills its slot, so a handle taken before it holds")
{
	AudioResources resources;
	resources.add_sound_bank("effects", SoundBank::silent());

	const AudioResources::SoundBankHandle handle =
		resources.resolve_sound_bank("effects");
	const SoundBank* first = resources.sound_bank(handle);

	resources.add_sound_bank("effects", SoundBank::silent());

	// A different object under the same name, reached through a handle resolved
	// before it existed. This is the whole reason a handle is a slot index and
	// not a pointer, and it is what lets audio_resources.h refuse to have a
	// reset_all_sounds(): a caller holding a SoundBank* across a reload is
	// holding a freed pointer, and a caller holding a handle is not.
	CHECK(resources.sound_bank(handle) != first);
	CHECK(resources.sound_bank(handle) == resources.sound_bank("effects"));
}

TEST_CASE("reading a bank through an unresolved handle throws")
{
	const AudioResources resources;
	const AudioResources::SoundBankHandle never_resolved;

	// Never nullptr, from either overload. A getter that answered null here
	// would be a crash one frame later in whichever object had asked.
	CHECK_THROWS_AS(std::ignore = resources.sound_bank(never_resolved),
		std::out_of_range);
	CHECK_THROWS_AS(std::ignore = resources.sound_bank("never added"),
		std::out_of_range);
}

TEST_CASE("a sound group is added once and resolved by name")
{
	AudioResources resources;
	const AudioResources::SoundGroupHandle music =
		resources.add_sound_group("music");
	const AudioResources::SoundGroupHandle effects =
		resources.add_sound_group("effects");

	CHECK(music.valid());
	CHECK(music != effects);
	CHECK(resources.resolve_sound_group("music") == music);
	CHECK(resources.resolve_sound_group("effects") == effects);

	// A group starts at full volume, so adding one changes no level.
	CHECK(resources.group_volume(music) == doctest::Approx(1.0f));
}

TEST_CASE("adding a group twice throws, and names it")
{
	AudioResources resources;
	resources.add_sound_group("music");

	CHECK_THROWS_WITH_AS(std::ignore = resources.add_sound_group("music"),
		doctest::Contains("'music'"), std::invalid_argument);
}

TEST_CASE("resolving a group nothing added throws, and names it")
{
	const AudioResources resources;

	CHECK_THROWS_WITH_AS(std::ignore = resources.resolve_sound_group("voice"),
		doctest::Contains("'voice'"), std::out_of_range);
}

TEST_CASE("a group volume is clamped into [0,1]")
{
	AudioResources resources;
	const AudioResources::SoundGroupHandle music =
		resources.add_sound_group("music");

	resources.set_group_volume(music, 0.25f);
	CHECK(resources.group_volume(music) == doctest::Approx(0.25f));

	resources.set_group_volume(music, 4.0f);
	CHECK(resources.group_volume(music) == doctest::Approx(1.0f));

	resources.set_group_volume(music, -1.0f);
	CHECK(resources.group_volume(music) == doctest::Approx(0.0f));
}

TEST_CASE("a group handle nothing issued is refused")
{
	AudioResources resources;
	resources.add_sound_group("music");
	const AudioResources::SoundGroupHandle never_resolved;
	const AudioResources::SoundGroupHandle past_the_end(5);

	CHECK_THROWS_AS(resources.set_group_volume(never_resolved, 0.5f),
		std::out_of_range);
	CHECK_THROWS_AS(resources.set_group_volume(past_the_end, 0.5f),
		std::out_of_range);
	CHECK_THROWS_AS(std::ignore = resources.group_volume(never_resolved),
		std::out_of_range);
}

TEST_CASE("a bank with no content checks a group above its content, and keeps nothing")
{
	AudioResources resources;
	resources.add_sound_bank("effects", SoundBank::silent());
	const AudioResources::SoundGroupHandle music =
		resources.add_sound_group("music");
	SoundBank* bank = resources.sound_bank("effects");

	const SoundBank::WaveHandle wave = bank->resolve_wave("shot");
	const SoundBank::EffectHandle effect = bank->resolve_effect("engine_loop");

	// The same rule as every other handle on this class: an unresolved
	// group is a program's mistake, refused whatever content is on disk.
	const AudioResources::SoundGroupHandle never_resolved;
	CHECK_THROWS_AS(bank->set_wave_group(wave, never_resolved),
		std::out_of_range);
	CHECK_THROWS_AS(bank->set_effect_group(effect, never_resolved),
		std::out_of_range);
	CHECK_THROWS_AS(bank->set_wave_group(SoundBank::WaveHandle(), music),
		std::out_of_range);
	CHECK_THROWS_AS(bank->set_effect_group(SoundBank::EffectHandle(), music),
		std::out_of_range);

	// A good group is validated and discarded, and a group volume change
	// then has nothing in this bank to reach.
	CHECK_NOTHROW(bank->set_wave_group(wave, music));
	CHECK_NOTHROW(bank->set_effect_group(effect, music));
	CHECK_NOTHROW(resources.set_group_volume(music, 0.5f));
	CHECK_NOTHROW(bank->play_wave(wave));
	CHECK_NOTHROW(bank->play_effect(effect, true));
}
