#pragma once

#include <cstdint>

namespace labrador
{
	// Which system commands the window answers itself rather than passing to
	// DefWindowProc. It is out of window.cpp so AppTests can pin it without
	// making Windows enter a menu loop, which is what the message does when
	// it is passed on.

	// Whether a WM_SYSCOMMAND is the keyboard asking for the menu with no key
	// named: what F10, or Alt pressed and released alone, sends. The low four
	// bits of `w_param` are the system's and are masked off, as the message's
	// contract requires. Alt+Space names ' ' and is not this.
	bool is_bare_menu_activation(uint64_t w_param, intptr_t l_param);
}
