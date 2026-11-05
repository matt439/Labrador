#pragma once

#include "engine/input/keyboard.h"

namespace labrador
{
	// The physical sides of Shift, Control and Alt behind the one public key
	// each, kept per window by both platform windows. update() records a
	// side and returns whether to forward that key event: a release is
	// withheld while the other side of the same modifier remains down, a
	// repeated down sets a side that is already set, and every other key is
	// forwarded. Clear when the window loses focus, where releases go
	// elsewhere.
	class ModifierSides
	{
	public:
		bool update(Key key, bool right, bool down);
		void clear();

	private:
		unsigned int held_ = 0;
	};
}
