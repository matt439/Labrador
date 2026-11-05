#include "samples/collision/pads.h"

namespace collision
{
	PadsInput combine_pads(std::span<const PadReading> slots)
	{
		PadsInput input;
		for (const PadReading& slot : slots)
		{
			input.stick += slot.stick;
			input.reset_requested = input.reset_requested || slot.reset_pressed;
			input.quit_requested = input.quit_requested || slot.quit_pressed;
		}
		return input;
	}
}
