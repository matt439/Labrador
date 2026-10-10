#pragma once

#include "engine/input/keyboard.h"
#include "engine/input/mouse.h"

#include <string_view>

namespace labrador
{
	// What the DOM's input events mean to the engine: the browser half of the
	// message translation engine/app/win32/window.cpp does for Win32. It is
	// out of web/window.cpp so that AppTests can pin it under Node, which has
	// no DOM to deliver an event from and so cannot construct a web Window.

	// KeyboardEvent.code, which names a physical position rather than a
	// character - "KeyZ" is the key right of the left Shift whatever the
	// layout prints on it - as the engine's Key. The same rule the Win32
	// window reads from a scan code, so a binding means the same key in both.
	// A position the engine has no name for is Key::none.
	Key key_from_code(std::string_view code);

	// The character a keydown types, from KeyboardEvent.key, or zero when it
	// types none.
	//
	// `key` is one character for a key that types one and a name ("Enter",
	// "ArrowLeft", "Dead") for every other, so only a single code point is
	// text. A chord is not text either, by the rule Win32's WM_SYSCHAR follows
	// for Alt: Control or Alt alone, or Meta, is a command. Control and Alt
	// together are AltGr, which is how half the world types "@", so that one
	// is text.
	char32_t text_from_key(std::string_view key, bool control, bool alt,
		bool meta);

	// Whether the browser keeps this key's own default action.
	//
	// The canvas keeps every key a player presses on its own, because an
	// unmodified key's default acts on the page under the game: the arrows,
	// Space and Page Down scroll it, and Firefox opens its find bar on "/".
	// It gives back what belongs to the browser and the system - every chord
	// with Control, Alt or Meta, so reload, close-tab and back still work,
	// the function keys, and Tab, which is how a keyboard user leaves the
	// canvas. Handling a key is not the same as consuming it, which is the
	// rule the Win32 window keeps for Alt+F4.
	bool browser_keeps_key(Key key, bool control, bool alt, bool meta);

	// MouseEvent.button, as the engine's MouseButton: 0 left, 1 middle,
	// 2 right, 3 and 4 the thumb buttons. Anything else is MouseButton::none.
	MouseButton mouse_button_from_dom(int button);

	// A WheelEvent delta as notches, keeping the DOM's sign: positive scrolls
	// the page down, or right. That is WM_MOUSEHWHEEL's sign for the
	// horizontal wheel and the opposite of WM_MOUSEWHEEL's for the vertical
	// one, so the vertical is the one the caller negates.
	//
	// The DOM reports pixels, lines or pages, and a notch is none of them.
	// A notch is taken as 100 pixels, which is Chrome's and Edge's on
	// Windows, or as 3 lines, which is Windows' own default; other browsers
	// come out within a factor of two, so a game reading fractional notches
	// sees a wheel that turns faster or slower, never one that turns
	// backwards. A page is one notch.
	float wheel_notches(double delta, unsigned int delta_mode);
}
