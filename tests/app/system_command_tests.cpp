#include <doctest/doctest.h>

#include "engine/app/win32/system_command.h"

#include <Windows.h>

TEST_CASE("only a bare keyboard menu activation is the window's to swallow")
{
	using labrador::is_bare_menu_activation;

	// What F10, or Alt alone, sends. The low four bits are the system's.
	CHECK(is_bare_menu_activation(SC_KEYMENU, 0));
	CHECK(is_bare_menu_activation(SC_KEYMENU | 0x5, 0));
	CHECK(is_bare_menu_activation(SC_KEYMENU | 0xF, 0));

	// Alt+Space names its key and opens the system menu, and Alt plus a
	// letter names that letter.
	CHECK_FALSE(is_bare_menu_activation(SC_KEYMENU, ' '));
	CHECK_FALSE(is_bare_menu_activation(SC_KEYMENU, 'f'));

	// Every other system command is the system's.
	CHECK_FALSE(is_bare_menu_activation(SC_CLOSE, 0));
	CHECK_FALSE(is_bare_menu_activation(SC_MAXIMIZE, 0));
	CHECK_FALSE(is_bare_menu_activation(SC_MOUSEMENU, 0));

	// No live case. Passed to DefWindowProc, SC_KEYMENU returned at once
	// without entering the menu loop for a hidden window, and for a shown
	// one that was never activated, so a message sent to any window here
	// passes whether the window swallows it or not.
}
