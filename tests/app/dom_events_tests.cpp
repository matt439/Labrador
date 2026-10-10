#include <doctest/doctest.h>

#include "engine/app/web/dom_events.h"

#include <set>
#include <string_view>

using namespace labrador;

// The browser half of the window's message translation, asserted with no
// browser. The Win32 half is window_tests.cpp, which drives a real window
// through SendMessageW; Node has no DOM to dispatch an event from, so the
// functions the web window calls with each event's fields are asserted here
// directly. Compiled into the browser build only, beside the file it tests.

TEST_CASE("a key code is a physical position, as a scan code is on Windows")
{
	// What a layout prints is not what a code names. An AZERTY board prints
	// A where QWERTY has Q and a QWERTZ one Z where QWERTY has Y, and the
	// codes still say KeyQ and KeyY - which is the rule LineSweeper's
	// bindings depend on, and the cases window_tests.cpp makes with scan
	// codes.
	CHECK(key_from_code("KeyQ") == Key::q);
	CHECK(key_from_code("KeyY") == Key::y);
	CHECK(key_from_code("KeyZ") == Key::z);
	CHECK(key_from_code("Semicolon") == Key::semicolon);

	// The navigation cluster and the numpad are different keys, as the
	// extended bit makes them on Windows.
	CHECK(key_from_code("Home") == Key::home);
	CHECK(key_from_code("Numpad7") == Key::numpad_7);
	CHECK(key_from_code("Delete") == Key::del);
	CHECK(key_from_code("NumpadDecimal") == Key::numpad_decimal);
	CHECK(key_from_code("NumpadDivide") == Key::numpad_divide);

	// Both halves of the pairs the engine does not tell apart.
	CHECK(key_from_code("ShiftLeft") == Key::shift);
	CHECK(key_from_code("ShiftRight") == Key::shift);
	CHECK(key_from_code("ControlRight") == Key::control);
	CHECK(key_from_code("AltRight") == Key::alt);
	CHECK(key_from_code("NumpadEnter") == Key::enter);

	CHECK(key_from_code("Pause") == Key::pause);
	CHECK(key_from_code("NumLock") == Key::num_lock);
	CHECK(key_from_code("PrintScreen") == Key::print_screen);

	// Positions the engine has no name for, and things that are not codes.
	CHECK(key_from_code("MetaLeft") == Key::none);
	CHECK(key_from_code("IntlBackslash") == Key::none);
	CHECK(key_from_code("F13") == Key::none);
	CHECK(key_from_code("keyz") == Key::none);
	CHECK(key_from_code("") == Key::none);
}

TEST_CASE("every key the engine names is reachable from a code")
{
	// The UI Events code for each of them, written out from the spec rather
	// than from the table it checks: a key with no code would be a key a
	// browser player can never press.
	constexpr std::string_view codes[] = {
		"KeyA", "KeyB", "KeyC", "KeyD", "KeyE", "KeyF", "KeyG", "KeyH",
		"KeyI", "KeyJ", "KeyK", "KeyL", "KeyM", "KeyN", "KeyO", "KeyP",
		"KeyQ", "KeyR", "KeyS", "KeyT", "KeyU", "KeyV", "KeyW", "KeyX",
		"KeyY", "KeyZ",
		"Digit0", "Digit1", "Digit2", "Digit3", "Digit4",
		"Digit5", "Digit6", "Digit7", "Digit8", "Digit9",
		"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11",
		"F12",
		"Escape", "Tab", "CapsLock", "ShiftLeft", "ControlLeft", "AltLeft",
		"Space", "Enter", "Backspace",
		"Insert", "Delete", "Home", "End", "PageUp", "PageDown",
		"ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown",
		"PrintScreen", "ScrollLock", "Pause", "NumLock",
		"Minus", "Equal", "BracketLeft", "BracketRight", "Backslash",
		"Semicolon", "Quote", "Backquote", "Comma", "Period", "Slash",
		"Numpad0", "Numpad1", "Numpad2", "Numpad3", "Numpad4",
		"Numpad5", "Numpad6", "Numpad7", "Numpad8", "Numpad9",
		"NumpadAdd", "NumpadSubtract", "NumpadMultiply", "NumpadDivide",
		"NumpadDecimal",
	};

	std::set<Key> reached;
	for (const std::string_view code : codes)
	{
		const Key key = key_from_code(code);
		CAPTURE(code);
		CHECK(key != Key::none);
		CHECK(reached.insert(key).second);
	}

	CHECK(reached.size() == static_cast<std::size_t>(Key::count) - 1);
}

TEST_CASE("a keydown types its key's one character, and a chord types nothing")
{
	CHECK(text_from_key("z", false, false, false) == U'z');
	CHECK(text_from_key("Z", false, false, false) == U'Z');
	CHECK(text_from_key(" ", false, false, false) == U' ');

	// UTF-8 in, one code point out, past the basic plane included - the
	// browser assembles what WM_CHAR delivers as a surrogate pair.
	CHECK(text_from_key("\xC3\xA9", false, false, false) == U'é');
	CHECK(text_from_key("\xE6\xBC\xA2", false, false, false) == U'漢');
	CHECK(text_from_key("\xF0\x9F\x9A\x80", false, false, false) ==
		U'\U0001F680');

	// A name is not a character.
	CHECK(text_from_key("Enter", false, false, false) == 0);
	CHECK(text_from_key("ArrowLeft", false, false, false) == 0);
	CHECK(text_from_key("Dead", false, false, false) == 0);
	CHECK(text_from_key("", false, false, false) == 0);

	// Control or Alt alone, or Meta, is a command: Control+C copies, and
	// Alt+F opens a menu, which is why Win32's WM_SYSCHAR is not text either.
	CHECK(text_from_key("c", true, false, false) == 0);
	CHECK(text_from_key("f", false, true, false) == 0);
	CHECK(text_from_key("c", false, false, true) == 0);

	// Both together is AltGr, which is how a German layout types "@".
	CHECK(text_from_key("@", true, true, false) == U'@');
	CHECK(text_from_key("@", true, true, true) == 0);

	// Bytes that are not one well-formed character: a truncated sequence, an
	// overlong slash, a lone surrogate, and two characters.
	CHECK(text_from_key("\xC3", false, false, false) == 0);
	CHECK(text_from_key("\xC0\xAF", false, false, false) == 0);
	CHECK(text_from_key("\xED\xA0\x80", false, false, false) == 0);
	CHECK(text_from_key("ab", false, false, false) == 0);
}

TEST_CASE("the canvas keeps the keys that act on the page, and gives back the browser's")
{
	// Unmodified, these would scroll the page under the game, or open
	// Firefox's find bar.
	for (const Key key : { Key::up, Key::down, Key::left, Key::right,
		Key::space, Key::page_down, Key::home, Key::slash, Key::apostrophe,
		Key::z, Key::escape, Key::enter, Key::shift, Key::backspace })
	{
		CAPTURE(static_cast<int>(key));
		CHECK_FALSE(browser_keeps_key(key, false, false, false));
	}

	// Reload, back and close-tab are the browser's, whatever key is chorded.
	CHECK(browser_keeps_key(Key::r, true, false, false));
	CHECK(browser_keeps_key(Key::left, false, true, false));
	CHECK(browser_keeps_key(Key::w, false, false, true));
	CHECK(browser_keeps_key(Key::z, true, true, false));

	// So are the function keys - refresh, full screen, the developer tools -
	// and Tab, which is how a keyboard user leaves the canvas, and any key
	// the engine has no name for.
	CHECK(browser_keeps_key(Key::f5, false, false, false));
	CHECK(browser_keeps_key(Key::f11, false, false, false));
	CHECK(browser_keeps_key(Key::f12, false, false, false));
	CHECK(browser_keeps_key(Key::tab, false, false, false));
	CHECK(browser_keeps_key(Key::none, false, false, false));
}

TEST_CASE("mouse buttons are numbered as MouseEvent.button numbers them")
{
	CHECK(mouse_button_from_dom(0) == MouseButton::left);
	CHECK(mouse_button_from_dom(1) == MouseButton::middle);
	CHECK(mouse_button_from_dom(2) == MouseButton::right);
	CHECK(mouse_button_from_dom(3) == MouseButton::x1);
	CHECK(mouse_button_from_dom(4) == MouseButton::x2);
	CHECK(mouse_button_from_dom(5) == MouseButton::none);
	CHECK(mouse_button_from_dom(-1) == MouseButton::none);
}

TEST_CASE("a wheel delta is notches, in the DOM's sign")
{
	// Pixels, lines and pages, at 100 pixels or 3 lines to the notch.
	CHECK(wheel_notches(100.0, 0) == doctest::Approx(1.0f));
	CHECK(wheel_notches(-100.0, 0) == doctest::Approx(-1.0f));
	CHECK(wheel_notches(50.0, 0) == doctest::Approx(0.5f));
	CHECK(wheel_notches(3.0, 1) == doctest::Approx(1.0f));
	CHECK(wheel_notches(-6.0, 1) == doctest::Approx(-2.0f));
	CHECK(wheel_notches(1.0, 2) == doctest::Approx(1.0f));

	// A mode the DOM has not defined moves nothing.
	CHECK(wheel_notches(100.0, 7) == 0.0f);
}
