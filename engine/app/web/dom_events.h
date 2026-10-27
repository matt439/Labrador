#pragma once

#include "engine/input/keyboard.h"
#include "engine/input/mouse.h"
#include "engine/math/vector2i.h"

#include <cstdint>
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

	// Per-window physical modifier sides behind the aggregate public keys.
	// update() records a side and returns whether to forward that key event;
	// a release is withheld while its other side remains down. Repeats are
	// idempotent. Clear when the canvas loses focus or the page is hidden.
	class DomModifiers
	{
	public:
		bool update(std::string_view code, bool down);
		void clear();
	private:
		unsigned int held_ = 0;
	};

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

	// What a PointerEvent on the canvas is to the touch device.
	enum class DomTouch : uint8_t
	{
		// Not touch, or nothing the device is told: a mouse or a pen, which
		// stay the mouse and arrive as the mouse events the browser makes of
		// them, or a pointer event type that says nothing about the contact.
		none,
		down,
		move,
		up,
		// The browser took the contact away - pointercancel - or the canvas
		// lost the capture that a touch takes on the element it starts on,
		// which means the rest of the contact will be delivered somewhere
		// else. Capture is also lost after every pointerup, when the point is
		// already gone and a cancel for it does nothing.
		cancel,
	};

	// From PointerEvent.type and PointerEvent.pointerType. Only "touch" is
	// touch.
	//
	// A touch the canvas handles has its default prevented, and that is what
	// keeps one tap one input: a browser makes compatibility mouse events from
	// a touch whose pointerdown was not cancelled, and they would be a click
	// in the same frame as the tap.
	DomTouch touch_from_pointer_event(std::string_view type,
		std::string_view pointer_type);

	// A position measured from the canvas's top-left corner in CSS pixels, as
	// client pixels of the drawing buffer: scaled by the buffer's size over
	// the box's, and floored, so a position just left of or above the canvas
	// is negative rather than rounded onto its first column.
	Vector2I client_pixels(double css_x, double css_y,
		double scale_x, double scale_y);
}
