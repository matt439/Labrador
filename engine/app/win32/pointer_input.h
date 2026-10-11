#pragma once

#include <cstdint>

namespace labrador
{
	// What WM_POINTER messages from a touch contact mean, and which mouse
	// messages are Windows' copies of one: the touch half of the message
	// translation window.cpp does. It is out of that file so AppTests can pin
	// it, because a test cannot send a WM_POINTER message the window will
	// believe - the window asks Windows what kind of pointer an id names, and
	// a made-up id is no kind at all.

	enum class TouchMessage : uint8_t
	{
		// Nothing for the touch device: a message that is not a pointer
		// message, or a pointer hovering rather than in contact.
		none,
		down,
		move,
		up,
		// Ended by the system rather than lifted: Windows marked the up as
		// cancelled, or the window lost capture of the pointer.
		cancel,
	};

	// `message` is a WM_POINTER message for a contact already known to be
	// touch, and `w_param` is its WPARAM, whose high word carries the
	// pointer's flags.
	TouchMessage touch_message(unsigned int message, uint64_t w_param);

	// Whether a mouse message was synthesised by Windows from a touch, read
	// from GetMessageExtraInfo. Windows marks every mouse message it makes
	// from a pen or a touch with one signature in the upper bits, and sets
	// one more bit for touch. Only touch is dropped: a pen stays the mouse,
	// because it is one precise pointer that hovers and clicks, which is what
	// the mouse device already describes.
	bool mouse_message_is_from_touch(intptr_t extra_info);
}
