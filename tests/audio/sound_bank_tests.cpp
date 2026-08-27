#include <doctest/doctest.h>

#include "engine/audio/audio_device.h"
#include "engine/audio/sound_bank.h"

#include <memory>
#include <stdexcept>
#include <tuple>

using namespace labrador;

// The bank a missing container produces, and what it still answers.
//
// WHAT THIS FILE USED TO BE, AND WHY IT IS SHORTER NOW.
// docs/survey/2026-08-26.md 3.4a asked for the written list of which of
// SoundBank's fourteen public methods SoundBank::silent() leaves observable,
// and this file was that list: eight of the thirteen instance methods had no
// observable behaviour anywhere in this repository, "it did not throw" was
// the whole of what a case could assert about them, and five sites of level
// clamping sat below the check for the platform and had never executed. The
// reason was not the tests. An audible SoundBank was UNCONSTRUCTIBLE here -
// its other constructor took a std::unique_ptr<DirectX::WaveBank>,
// DirectXTK's WaveBank has exactly one constructor taking a path to an .xwb,
// and there is no .xwb in this tree.
//
// 3.4b IS WHAT THAT LIST WAS FOR AND IT HAS BEEN SPENT. engine/audio/ has a
// seam now (audio_device.h) and a headless implementation behind it
// (engine/audio/null/), so all thirteen methods have observable behaviour in
// the configuration that builds it - tests/audio/null_tests.cpp is where the
// eight say what they did, and where the clamp is read back off the numbers
// that reached the device. What is left here is the other bank, and it is a
// different question: not "what can a test see", but what a game gets when the
// wave bank a manifest marked optional is not on disk.
//
// SO EVERY CASE BELOW IS ABOUT SoundBank::silent() ON PURPOSE, rather than for
// want of an alternative. It is the substitute the loader installs for a
// missing container, it is reachable in every configuration, and the two
// promises it cannot keep are stated in sound_bank.h as deliberate trades. What
// changed in this commit is the third one, which it CAN now keep: a handle
// nobody resolved is refused by every bank, because the check for it moved
// above the check for content.

TEST_CASE("a bank with no content says so, and nothing else does")
{
	const std::unique_ptr<SoundBank> bank = SoundBank::silent();

	// The whole of what audible() is for. Every other method reads it and then
	// says nothing about it, so this is the only line in the class a caller can
	// use to find out which kind of bank it was handed.
	//
	// IT IS NOT A QUESTION ABOUT THE BUILD, which is worth asserting in the
	// file most likely to be misread as saying it is. Under the null audio
	// backend a loaded bank is audible and still makes no sound: what this
	// answers is whether there was a container to open.
	CHECK(bank->audible() == false);
}

TEST_CASE("a bank with no content resolves any name, including an impossible one")
{
	const std::unique_ptr<SoundBank> bank = SoundBank::silent();

	const SoundBank::WaveHandle wave = bank->resolve_wave("no such wave");
	const SoundBank::EffectHandle effect =
		bank->resolve_effect("no such effect");

	CHECK(wave.valid());
	CHECK(effect.valid());

	// Slot zero rather than an unresolved handle, and sound_bank.cpp says why
	// in its own words: every play below rejects an unresolved handle in every
	// build now, and a bank with no content must not reject a name.
	CHECK(wave.index() == 0);
	CHECK(effect.index() == 0);
}

TEST_CASE("a bank with no content cannot keep the promise resolving exists to make")
{
	const std::unique_ptr<SoundBank> bank = SoundBank::silent();

	// sound_bank.h states this trade out loud, and it is the reason silent() is
	// a named constructor a loader has to ask for rather than a state the class
	// can fall into. It is asserted here because the entire T6 guarantee of
	// this class is a throw that does not happen on the next two lines: with a
	// container present a misspelt wave name fails at load naming the wave, and
	// with it absent the same typo is nothing at all.
	CHECK_NOTHROW(std::ignore = bank->resolve_wave("bang"));
	CHECK_NOTHROW(std::ignore = bank->resolve_wave("bagn"));

	// And the two spellings are not merely both accepted, they are the same
	// answer. There is no name table to check against, so there is nothing to
	// hand back that could tell them apart.
	CHECK(bank->resolve_wave("bang").index() ==
		bank->resolve_wave("bagn").index());
}

TEST_CASE("a bank with no content plays nothing, and every verb still runs")
{
	const std::unique_ptr<SoundBank> bank = SoundBank::silent();
	const SoundBank::WaveHandle wave = bank->resolve_wave("shot");
	const SoundBank::EffectHandle effect = bank->resolve_effect("engine_loop");

	// Still the whole of what can be asserted about these here, and it is no
	// longer the whole of what can be asserted about them: null_tests.cpp reads
	// each one back off a device. What this case pins is narrower and is the
	// contract that survives a missing file - a game that is otherwise correct
	// runs in silence rather than throwing on a file it was never going to
	// have.
	CHECK_NOTHROW(bank->play_wave(wave));
	CHECK_NOTHROW(bank->play_effect(effect, true));
	CHECK_NOTHROW(bank->stop_effect(effect));
	CHECK_NOTHROW(bank->stop_effect(effect, true));
	CHECK_NOTHROW(bank->pause_effect(effect));
	CHECK_NOTHROW(bank->resume_effect(effect));
	CHECK_NOTHROW(bank->set_effect_volume(effect, 0.5f));
	CHECK_NOTHROW(bank->set_effect_pitch(effect, 0.5f));
	CHECK_NOTHROW(bank->set_effect_pan(effect, 0.5f));

	// The two readers answer rather than throwing, and what they answer is
	// true: nothing is playing, because nothing can.
	CHECK(bank->effect_state(effect) == SoundState::stopped);
	CHECK(bank->is_effect_looping(effect) == false);
}

TEST_CASE("a handle nobody resolved is refused by a bank with no content too")
{
	const std::unique_ptr<SoundBank> bank = SoundBank::silent();

	// Default-constructed, so unresolved.
	const SoundBank::WaveHandle never_resolved_wave;
	const SoundBank::EffectHandle never_resolved_effect;

	// THIS IS THE CASE THAT CHANGED, AND IT IS WHAT THE REORDER BOUGHT. Both
	// guards used to sit BELOW the check for content, so the one mistake this
	// class exists to catch loudly - a handle nobody resolved - was caught in a
	// build with audio and went unmentioned in a build without it. 3.4a
	// recorded that as a cost of where the seam was drawn. Drawing it properly
	// moved every engine-side check above the platform-side one, and a program
	// bug now fails the same way whatever content is on disk.
	CHECK_THROWS_AS(bank->play_wave(never_resolved_wave), std::out_of_range);
	CHECK_THROWS_AS(bank->play_effect(never_resolved_effect),
		std::out_of_range);
	CHECK_THROWS_AS(bank->stop_effect(never_resolved_effect),
		std::out_of_range);
	CHECK_THROWS_AS(bank->pause_effect(never_resolved_effect),
		std::out_of_range);
	CHECK_THROWS_AS(bank->resume_effect(never_resolved_effect),
		std::out_of_range);
	CHECK_THROWS_AS(bank->set_effect_volume(never_resolved_effect, 0.5f),
		std::out_of_range);
	CHECK_THROWS_AS(bank->set_effect_pitch(never_resolved_effect, 0.5f),
		std::out_of_range);
	CHECK_THROWS_AS(bank->set_effect_pan(never_resolved_effect, 0.5f),
		std::out_of_range);
	CHECK_THROWS_AS(std::ignore = bank->effect_state(never_resolved_effect),
		std::out_of_range);
	CHECK_THROWS_AS(
		std::ignore = bank->is_effect_looping(never_resolved_effect),
		std::out_of_range);
}

TEST_CASE("a bank with no content still runs the arithmetic it owns")
{
	const std::unique_ptr<SoundBank> bank = SoundBank::silent();
	const SoundBank::WaveHandle wave = bank->resolve_wave("shot");
	const SoundBank::EffectHandle effect = bank->resolve_effect("engine_loop");

	// Nonsense on every axis - a volume five hundred times full, a pitch and a
	// pan an order of magnitude outside their range - and none of it is
	// refused, because there is nothing to play it.
	//
	// WHAT THIS CASE CANNOT SEE, AND WHY IT IS STILL HERE. The clamp runs
	// now: it sits above the check for content rather than below it, which is
	// where docs/survey/2026-08-26.md 3.4a measured that it belonged, because
	// folding a volume into [0,1] is arithmetic this module owns and not the
	// platform's. A bank with nothing behind it still cannot show you the
	// result - there is no device to have received the clamped number. That
	// is what null_tests.cpp is for, and the split between these two files is
	// the honest statement of what a seam bought: the order is a claim about
	// which side of the wall the arithmetic is on, and the headless backend
	// is what makes the claim checkable.
	CHECK_NOTHROW(bank->play_wave(wave, 500.0f, -12.0f, 9.0f));
	CHECK_NOTHROW(bank->play_effect(effect, false, -1.0f, 40.0f, -40.0f));
	CHECK_NOTHROW(bank->set_effect_volume(effect, 1000.0f));
	CHECK_NOTHROW(bank->set_effect_pitch(effect, -1000.0f));
	CHECK_NOTHROW(bank->set_effect_pan(effect, 1000.0f));
}
