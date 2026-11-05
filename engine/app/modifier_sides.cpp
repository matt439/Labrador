#include "engine/app/modifier_sides.h"

namespace labrador
{
	namespace
	{
		// Two bits per modifier, its left side and then its right.
		constexpr unsigned int shift_bits = 0;
		constexpr unsigned int control_bits = 2;
		constexpr unsigned int alt_bits = 4;
		constexpr unsigned int both_sides = 3u;
	}

	bool ModifierSides::update(Key key, bool right, bool down)
	{
		unsigned int first = 0;
		switch (key)
		{
		case Key::shift: first = shift_bits; break;
		case Key::control: first = control_bits; break;
		case Key::alt: first = alt_bits; break;
		default: return true;
		}

		const unsigned int bit = 1u << (first + (right ? 1u : 0u));
		if (down)
		{
			this->held_ |= bit;
			return true;
		}
		this->held_ &= ~bit;
		return (this->held_ & (both_sides << first)) == 0;
	}

	void ModifierSides::clear()
	{
		this->held_ = 0;
	}
}
