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
// AudioResources is a Registry<SoundBank> and four forwarding methods, so most
// of what is asserted here is the registry contract - and that is the point of
// asserting it. tests/core/registry_tests.cpp pins the template; this pins that
// the audio module gets the same bargain as the render one, which is the claim
// audio_resources.h opens with when it calls itself "the audio counterpart to
// RenderResources".
//
// Every bank below is SoundBank::silent(), for the reason sound_bank_tests.cpp
// gives at length: it is the only SoundBank this repository can construct. That
// costs this file nothing, because none of it is about what a bank plays.

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
