#pragma once

#include "engine/core/handle.h"

namespace labrador
{
	// A named group of sounds whose volume is set as one - music, effects, a
	// game's voice lines. AudioResources holds the groups and their volumes,
	// and a SoundBank multiplies a sound's group volume into every level it
	// sends to the device (audio_resources.h, sound_bank.h).
	//
	// The tag is never defined, like SoundBank::Wave: it exists so a handle to
	// a group cannot be passed where a handle to a bank or a wave belongs
	// (core/handle.h). It is in a header of its own because sound_bank.h and
	// audio_resources.h both name it, and the second includes the first.
	//
	// A handle belongs to the AudioResources that issued it. A handle from
	// another one is an index into the wrong table, and nothing can tell it
	// apart from a valid one.
	struct SoundGroup;
	using SoundGroupHandle = Handle<SoundGroup>;
}
