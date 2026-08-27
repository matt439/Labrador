#include <doctest/doctest.h>

#include "engine/audio/audio_resources.h"
#include "engine/audio/sound_bank.h"
#include "engine/audio/sound_bank_object.h"

#include <Audio.h>

#include <stdexcept>
#include <string>
#include <tuple>

using namespace labrador;

// The base class nothing in this repository inherits.
//
// SoundBankObject is plumbing for a game object that makes a noise, and both
// clients that would inherit it are in the other repository - so it is a public
// name with no caller here, which PHILOSOPHY.md:612-621 says is a count that
// settles nothing. What it is not is a reason to leave the type unexercised,
// and constructing it once turned up something a grep could not: a
// default-constructed SoundBankObject used to dereference a null pointer on
// every call, and there is no setter that could ever have repaired it. The last
// case in this file is that finding, now that it says so instead.
namespace
{
	// The whole surface is protected, because this class is inherited for the
	// plumbing rather than held. Reaching it therefore needs the subclass it
	// was written for, and this is that subclass and nothing else: it adds no
	// behaviour, so what a case observes is SoundBankObject's own.
	class NoisyObject : public SoundBankObject
	{
	public:
		NoisyObject() = default;
		using SoundBankObject::SoundBankObject;

		using SoundBankObject::is_effect_looping;
		using SoundBankObject::effect_state;
		using SoundBankObject::pause_effect;
		using SoundBankObject::play_effect;
		using SoundBankObject::play_wave;
		using SoundBankObject::resolve_effect;
		using SoundBankObject::resolve_wave;
		using SoundBankObject::resume_effect;
		using SoundBankObject::set_effect_pan;
		using SoundBankObject::set_effect_pitch;
		using SoundBankObject::set_effect_volume;
		using SoundBankObject::set_sound_bank;
		using SoundBankObject::sound_bank;
		using SoundBankObject::stop_effect;
	};
}

TEST_CASE("an object resolves its bank name while it is being built")
{
	AudioResources resources;
	resources.add_sound_bank("effects", SoundBank::silent());

	const NoisyObject object("effects", &resources);

	CHECK(object.sound_bank() == resources.sound_bank("effects"));
}

TEST_CASE("a bank name nothing loaded stops the object being built at all")
{
	const AudioResources resources;

	// sound_bank_object.h:36-38 promises this: names go in at construction and
	// handles come out, so a misspelt bank fails while the menu is being built
	// rather than staying silent on the press that wanted it. The subject here
	// is the constructor rather than a method, which is the part that makes it
	// load-time (T6) - there is no half-built object left over to call.
	CHECK_THROWS_AS(NoisyObject("effets", &resources), std::out_of_range);
}

TEST_CASE("every play call an object forwards reaches its bank")
{
	AudioResources resources;
	resources.add_sound_bank("effects", SoundBank::silent());

	const NoisyObject object("effects", &resources);
	const SoundBank::WaveHandle wave = object.resolve_wave("shot");
	const SoundBank::EffectHandle effect = object.resolve_effect("engine_loop");

	// Twelve forwards, each one line, and against a silent bank eight of them
	// can only be checked for arriving somewhere at all - which is
	// sound_bank_tests.cpp's finding inherited rather than a second one. What
	// this case does establish is that none of the twelve forgets to go
	// through sound_bank(): a forward that read the wrong member would throw
	// here rather than in a client.
	CHECK_NOTHROW(object.play_wave(wave));
	CHECK_NOTHROW(object.play_effect(effect, true));
	CHECK_NOTHROW(object.stop_effect(effect));
	CHECK_NOTHROW(object.pause_effect(effect));
	CHECK_NOTHROW(object.resume_effect(effect));
	CHECK_NOTHROW(object.set_effect_volume(effect, 0.5f));
	CHECK_NOTHROW(object.set_effect_pitch(effect, 0.5f));
	CHECK_NOTHROW(object.set_effect_pan(effect, 0.5f));

	CHECK(object.effect_state(effect) == DirectX::SoundState::STOPPED);
	CHECK(object.is_effect_looping(effect) == false);
}

TEST_CASE("set_sound_bank points the object at a different bank")
{
	AudioResources resources;
	resources.add_sound_bank("effects", SoundBank::silent());
	resources.add_sound_bank("music", SoundBank::silent());

	NoisyObject object("effects", &resources);
	CHECK(object.sound_bank() == resources.sound_bank("effects"));

	object.set_sound_bank("music");
	CHECK(object.sound_bank() == resources.sound_bank("music"));

	// What is not asserted, and cannot be here: the header warns that every
	// handle resolved from the old bank is meaningless against the new one. Two
	// silent banks resolve every name to slot zero, so the two are numerically
	// identical and the warning has nothing to show. It is true of a pair of
	// audible banks, which this repository cannot build.
	CHECK_THROWS_AS(object.set_sound_bank("voice"), std::out_of_range);
}

TEST_CASE("a default-constructed object says so rather than crashing")
{
	// FOUND BY WRITING THIS FILE, AND FIXED IN THE SAME COMMIT. The default
	// constructor exists so that a subclass can be default-constructed, which
	// every drawable object in this engine is. It leaves audio_resources_ null,
	// and nothing can ever fill it in - set_sound_bank changes which bank, not
	// which table - so an object built this way is permanently unusable, and
	// both ways of touching it used to be a null dereference.
	//
	// The throw is what the rest of the engine already does: Registry never
	// answers nullptr, it says which resource was missing and stops (T6). A
	// crash in a client's frame is the same information delivered somewhere
	// nobody can act on it.
	NoisyObject object;

	CHECK_THROWS_AS(std::ignore = object.sound_bank(), std::logic_error);
	CHECK_THROWS_AS(object.set_sound_bank("effects"), std::logic_error);
}
