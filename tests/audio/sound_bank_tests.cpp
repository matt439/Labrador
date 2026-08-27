#include <doctest/doctest.h>

#include "engine/audio/sound_bank.h"

#include <Audio.h>

#include <memory>
#include <tuple>

using namespace labrador;

// The audio module's first test, and what it is for.
//
// engine/audio/ was the only engine module with no ctest entry. docs/next.md
// section 3.4a asked for this target and named its product in advance: not
// coverage, but the written list of which of SoundBank's fourteen public
// methods SoundBank::silent() leaves observable. That list is below, it was
// taken by writing the cases under it, and it is the measured argument for how
// wide the audio seam section 3.4b describes has to be.
//
// WHY EVERY CASE HERE USES A SILENT BANK, AND WHY THAT IS NOT A CHOICE.
// SoundBank's other constructor takes a std::unique_ptr<DirectX::WaveBank>, and
// DirectXTK's WaveBank has exactly one constructor: an AudioEngine* and a path
// to an .xwb. There is no .xwb in this repository - `find . -name "*.xwb"` is
// empty, because the one the shipped manifest names is built from source audio
// that cannot be distributed (README, "Audio") - and
// DirectX::SoundEffectInstance has no public constructor at all, only a move,
// so the instance registry cannot be filled by hand either. An audible
// SoundBank is therefore not merely untested here. It is unconstructible.
//
// That is PHILOSOPHY.md:632-637 almost word for word - "a seam with only the
// platform's own implementation behind it still requires the platform in order
// to construct anything" - and it is the finding, because silent() looks like
// the headless implementation that rule demands and is not one. It is a null
// WaveBank pointer inside the platform's own class, checked at the top of every
// method. So it can decline to do things. It cannot record them the way
// render/null/ records a draw, and nothing in this file can assert that a sound
// was played, because nothing anywhere in this tree can observe that.
//
// THE LIST. Fourteen public methods, counting the named constructor. Of the
// thirteen instance methods, a silent bank leaves five with a return value to
// assert on and eight with nothing but "it did not throw".
//
//   Answers something          what a silent bank says
//     audible()                false, and this is the only method that
//                              distinguishes the two kinds of bank at all
//     resolve_wave(any)        handle 0, for every name, without throwing
//     resolve_effect(any)      handle 0, for every name, without throwing
//     effect_state(any)        STOPPED, even for a handle it never issued
//     is_effect_looping(any)   false, likewise
//
//   Answers nothing            what the audible() check skips
//     play_wave                the unresolved-handle throw, clamp_levels,
//                              WaveBank::Play
//     play_effect              clamp_levels, the registry read, SetVolume,
//                              SetPitch, SetPan, Play
//     stop_effect              Stop(immediate) - so `immediate` is inert
//     pause_effect             Pause
//     resume_effect            Resume
//     set_effect_volume        clamp to [0,1], SetVolume
//     set_effect_pitch         clamp to [-1,1], SetPitch
//     set_effect_pan           clamp to [-1,1], SetPan
//
// WHAT THE RIGHT-HAND COLUMN IS FOR. Every entry in it is a line of engine code
// that has never executed anywhere in this repository, and the level clamp is
// on five of the eight. That clamp is arithmetic this module owns - it is not
// XAudio2's and it is not DirectXTK's, and folding a volume into [0,1] is the
// same kind of engine-side decision as the glyph walk in render/font.h. It sits
// below the check for the platform rather than above it. A seam drawn where
// audible() is checked today would put it on the platform's side of the wall,
// which is the wrong side for it, and that is the one thing this file measures
// that a reader of sound_bank.cpp would otherwise have to count by hand.

TEST_CASE("a silent bank is the only bank this repository can construct")
{
	const std::unique_ptr<SoundBank> bank = SoundBank::silent();

	// The whole of what audible() is for. Every other method reads it and then
	// says nothing about it, so this is the only line in the class a caller can
	// use to find out which kind of bank it was handed.
	CHECK(bank->audible() == false);
}

TEST_CASE("a silent bank resolves any name, including one no bank could hold")
{
	const std::unique_ptr<SoundBank> bank = SoundBank::silent();

	const SoundBank::WaveHandle wave = bank->resolve_wave("no such wave");
	const SoundBank::EffectHandle effect =
		bank->resolve_effect("no such effect");

	CHECK(wave.valid());
	CHECK(effect.valid());

	// Slot zero rather than an unresolved handle, and sound_bank.cpp:35-38 says
	// why in its own words: play_wave rejects an unresolved handle, and a
	// silent bank must not reject anything.
	CHECK(wave.index() == 0);
	CHECK(effect.index() == 0);
}

TEST_CASE("a silent bank cannot keep the promise resolving exists to make")
{
	const std::unique_ptr<SoundBank> bank = SoundBank::silent();

	// sound_bank.h:41-48 states this trade out loud, and it is the reason
	// silent() is a named constructor a loader has to ask for rather than a
	// state the class can fall into. It is asserted here because the entire T6
	// guarantee of this class is a throw that does not happen on the next two
	// lines: with audio present a misspelt wave name fails at load naming the
	// wave, and with audio absent the same typo is nothing at all.
	CHECK_NOTHROW(std::ignore = bank->resolve_wave("bang"));
	CHECK_NOTHROW(std::ignore = bank->resolve_wave("bagn"));

	// And the two spellings are not merely both accepted, they are the same
	// answer. There is no name table to check against, so there is nothing to
	// hand back that could tell them apart.
	CHECK(bank->resolve_wave("bang").index() ==
		bank->resolve_wave("bagn").index());
}

TEST_CASE("all eight of a silent bank's void verbs run, and do nothing")
{
	const std::unique_ptr<SoundBank> bank = SoundBank::silent();
	const SoundBank::WaveHandle wave = bank->resolve_wave("shot");
	const SoundBank::EffectHandle effect = bank->resolve_effect("engine_loop");

	// There is nothing else to assert, and writing all eight out is the case
	// rather than an omission from it. Each returns at the audible() check and
	// a silent bank has no state a caller can read afterwards, so "it did not
	// throw" is the complete observable behaviour of eight of this class's
	// thirteen instance methods. That count is the argument in the list at the
	// top of this file, and this is where it was taken.
	CHECK_NOTHROW(bank->play_wave(wave));
	CHECK_NOTHROW(bank->play_effect(effect, true));
	CHECK_NOTHROW(bank->stop_effect(effect));
	CHECK_NOTHROW(bank->stop_effect(effect, true));
	CHECK_NOTHROW(bank->pause_effect(effect));
	CHECK_NOTHROW(bank->resume_effect(effect));
	CHECK_NOTHROW(bank->set_effect_volume(effect, 0.5f));
	CHECK_NOTHROW(bank->set_effect_pitch(effect, 0.5f));
	CHECK_NOTHROW(bank->set_effect_pan(effect, 0.5f));
}

TEST_CASE("a silent bank rejects nothing, not even a handle it never issued")
{
	const std::unique_ptr<SoundBank> bank = SoundBank::silent();

	// Default-constructed, so unresolved. On an audible bank three of the four
	// calls below reach Registry::get and throw naming the instance, and the
	// fourth - play_wave - has its own guard and its own sentence for it.
	const SoundBank::WaveHandle never_resolved_wave;
	const SoundBank::EffectHandle never_resolved_effect;

	// Both guards are below the audible() check, so this is the second promise
	// a silent bank cannot keep, and like the first it is deliberate. The cost
	// is worth stating: the one mistake this class exists to catch loudly, a
	// handle nobody resolved, is caught in a build that has audio and goes
	// unmentioned in a build that does not.
	CHECK_NOTHROW(bank->play_wave(never_resolved_wave));
	CHECK_NOTHROW(bank->play_effect(never_resolved_effect));

	// The two readers answer rather than throwing, and what they answer is
	// true: nothing is playing, because nothing can.
	CHECK(bank->effect_state(never_resolved_effect) ==
		DirectX::SoundState::STOPPED);
	CHECK(bank->is_effect_looping(never_resolved_effect) == false);
}

TEST_CASE("a silent bank does not clamp the levels it is handed")
{
	const std::unique_ptr<SoundBank> bank = SoundBank::silent();
	const SoundBank::WaveHandle wave = bank->resolve_wave("shot");
	const SoundBank::EffectHandle effect = bank->resolve_effect("engine_loop");

	// Nonsense on every axis - a volume five hundred times full, a pitch and a
	// pan an order of magnitude outside their range - and none of it is
	// refused, because the clamp that would have folded it back into range is
	// below the audible() check and does not run.
	//
	// This case asserts the weakest thing in the file and is the one most worth
	// keeping. It is not a claim that the clamp is wrong: the clamp is right,
	// and on an audible bank it is what stops a content bug becoming a burst of
	// noise. It is the record that five sites of arithmetic this module owns
	// are, in this tree, code no test can reach - which is the shape of finding
	// a seam exists to fix, and section 3.4b is where it gets spent.
	CHECK_NOTHROW(bank->play_wave(wave, 500.0f, -12.0f, 9.0f));
	CHECK_NOTHROW(bank->play_effect(effect, false, -1.0f, 40.0f, -40.0f));
	CHECK_NOTHROW(bank->set_effect_volume(effect, 1000.0f));
	CHECK_NOTHROW(bank->set_effect_pitch(effect, -1000.0f));
	CHECK_NOTHROW(bank->set_effect_pan(effect, 1000.0f));
}
