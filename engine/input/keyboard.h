#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace labrador
{
	// WHY THIS DEVICE IS FED AND THE PADS ARE READ.
	//
	// gamepads.h owns a GamepadReader and asks it for a complete snapshot of
	// every slot whenever it likes. There is no equivalent here. A keyboard
	// reaches a Win32 program as messages in the window's queue, and the one
	// channel below that genuinely needs them - typed text - cannot be
	// reconstructed from key state at all: key repeat, shift and caps
	// resolution, dead keys and IME composition are all performed by the OS
	// between WM_KEYDOWN and WM_CHAR, and none of it is visible in a bitmask.
	//
	// So the flow is app -> input rather than input -> backend. Window
	// translates the messages, Application forwards them here, and this class
	// accumulates. That direction is what keeps the module table intact:
	// `input` may depend on core and math, never on `app`, and it does not -
	// nothing in this file knows a window exists. It is also why there is no
	// input/win32/ folder for this device: the platform edge is the window,
	// which already exists and is already named as platform code in
	// ARCHITECTURE. Inventing a second one for a device that has no backend to
	// select would be the speculative framework T1 rules out.
	//
	// WHAT A GAME SEES IS STILL A POLL. Messages land continuously; poll()
	// latches them into a frame. Everything below reads exactly like
	// gamepads.h - state, previous_state, held, pressed, released - because an
	// edge is "down now, up last frame" whichever way the bits arrived, and a
	// client should not have to learn two shapes to ask one question.

	// Every key this engine names, one bit each in KeyboardState.
	//
	// It is a key POSITION, not a character. `Key::z` is the key a US layout
	// calls Z, whatever a French layout prints on it, because that is what a
	// movement binding wants. What was typed is typed(), which is the other
	// question and has the other answer.
	//
	// LEFT AND RIGHT MODIFIERS ARE NOT DISTINGUISHED, deliberately (T3). A
	// message carries VK_SHIFT and the side lives in the scan code, so telling
	// them apart is a second lookup that no client has asked for; `shift`,
	// `control` and `alt` each mean "either one". Numpad Enter reports as
	// `enter` for the same reason. Both are documented limits of a simple
	// model rather than oversights, and both are additive to fix.
	enum class Key : uint8_t
	{
		// Not a key. What an unmapped platform code translates to, so a
		// keyboard nobody anticipated cannot index a bit that means something
		// else.
		none = 0,

		a, b, c, d, e, f, g, h, i, j, k, l, m,
		n, o, p, q, r, s, t, u, v, w, x, y, z,

		// The number row. Named for what they are rather than what shift makes
		// them, and separate from the numpad below because a game binding one
		// almost never means the other.
		digit_0, digit_1, digit_2, digit_3, digit_4,
		digit_5, digit_6, digit_7, digit_8, digit_9,

		f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12,

		escape,
		tab,
		caps_lock,
		shift,
		control,
		alt,
		space,
		enter,
		backspace,

		insert,
		// `delete` is a keyword, and this is the one name in the set that
		// could not be spelt the obvious way.
		del,
		home,
		end,
		page_up,
		page_down,

		left,
		right,
		up,
		down,

		print_screen,
		scroll_lock,
		pause,
		num_lock,

		minus,
		equals,
		left_bracket,
		right_bracket,
		backslash,
		semicolon,
		apostrophe,
		grave,
		comma,
		period,
		slash,

		numpad_0, numpad_1, numpad_2, numpad_3, numpad_4,
		numpad_5, numpad_6, numpad_7, numpad_8, numpad_9,
		numpad_add,
		numpad_subtract,
		numpad_multiply,
		numpad_divide,
		numpad_decimal,

		// Not a key either. The bound every loop over the set stops at, and
		// the number the storage below is sized against.
		count,
	};

	// The keyboard, one frame. A value, because an edge detector needs two of
	// them and a test needs to be able to write one by hand.
	struct KeyboardState
	{
		// False means the window did not have the keyboard this frame, and
		// every key below is then up rather than whatever was last held.
		//
		// It is `connected` under another name, and it carries the same rule
		// (see pressed, below). A pad can be unplugged; a keyboard cannot, but
		// the window can lose the foreground, which produces exactly the same
		// hazard - the key-up for a key held while alt-tabbing away is
		// delivered to whatever took the focus, never to this window, so a
		// game that kept the bit set would walk left forever.
		bool focused = false;

		// One bit per Key, indexed by the enumerator. Two words because the
		// set is over sixty-four keys and under a hundred and twenty-eight;
		// the static_assert below is what keeps that true as keys are added.
		uint64_t keys[2] = {};

		// False for Key::none and Key::count, which are bounds rather than
		// keys - so a caller that forwards an unmapped translation straight in
		// gets a clean `no` rather than an out-of-range read.
		bool is_down(Key key) const;
	};

	static_assert(static_cast<unsigned int>(Key::count) <= 128,
		"KeyboardState::keys is two words. A key set past 128 needs a third.");

	// The edges between two frames.
	//
	// Free functions over two values, and Keyboard's methods of the same names
	// are forwarders to them - the same arrangement gamepad.h has, for the
	// same reason: it is what makes the whole of the edge logic testable
	// without a window, a message pump or a keyboard attached to the machine
	// running the test.
	//
	// AN EDGE REQUIRES THE WINDOW TO HAVE BEEN FOCUSED ON BOTH FRAMES, and
	// that conjunction is part of the contract rather than an implementation
	// detail. It is gamepad.h's rule transplanted, and it closes the same
	// three doors:
	//
	//  - The first poll of the process compares against a default-constructed
	//    previous, so a key already down at launch would read as pressed.
	//  - Focus returns and the window is told nothing about what is still
	//    held, so the first key-down after an alt-tab back would be a press
	//    for a key the player never released.
	//  - Losing focus clears every key at once (Keyboard::set_focused), so a
	//    whole chord would fire released() on the frame the window went away.
	//
	// None of it is a client's to guard, for the reason gamepad.h gives: the
	// obvious test is true on precisely the frame that is wrong.
	//
	// THE FOCUS-LOSS SIDE LOSES NOTHING BUT CHANGES SPELLING. A key held when
	// the window goes away no longer produces released() - held() is false
	// there too - so a client keying "stop firing" off a release asks for the
	// focus change instead:
	//
	//     just_unfocused(now, before) && before.is_down(Key::space)
	bool pressed(const KeyboardState& now, const KeyboardState& before,
		Key key);
	bool released(const KeyboardState& now, const KeyboardState& before,
		Key key);

	bool just_focused(const KeyboardState& now, const KeyboardState& before);
	bool just_unfocused(const KeyboardState& now, const KeyboardState& before);

	// The keyboard as it was this frame and last, the edges between them, and
	// what was typed in between.
	//
	// Application owns one, feeds it from the window's messages and polls it
	// once per frame before any state updates - which is what makes "down now,
	// up last frame" true for every reader, and what stops two states keeping
	// two edge detectors that advance on different frames (gamepads.h has the
	// bug that shape shipped).
	class Keyboard
	{
	public:
		// THE FEEDING SIDE. Whoever owns the platform's messages calls these;
		// on Windows that is Application, forwarding what Window translated.
		//
		// They are public rather than friended because the alternative is a
		// friend declaration naming a class in `app`, which is the module
		// `input` is forbidden to know about. It is also the seam a test
		// double or a replay source drives, and both want it reachable - the
		// tests beside this file construct a Keyboard and type into it with no
		// window anywhere.
		//
		// Nothing here is visible to a reader until the next poll(). A key
		// pressed and released between two polls is not seen at all, exactly
		// as it is not on a pad, and at sixty frames a second no human
		// produces one. Typed text is the exception and is accumulated rather
		// than sampled, because a fast typist genuinely does produce two
		// characters inside one frame.
		void on_key_down(Key key);
		void on_key_up(Key key);

		// One typed character, already decoded from whatever the platform's
		// encoding was. UTF-32 in, UTF-8 out: the assembly of surrogate pairs
		// is the message translator's job, so nothing here has to know that
		// Windows speaks UTF-16.
		//
		// Surrogates and anything past U+10FFFF are replaced with U+FFFD
		// rather than dropped, which is render/text_encoding.h's rule for the
		// same decision: text about to be drawn should show mojibake, not
		// vanish.
		//
		// CONTROL CHARACTERS ARE DROPPED - everything below U+0020, and U+007F.
		// The platform reports Enter, Tab, Escape and Backspace as characters
		// as well as keys, and a name field that appended them would collect a
		// 0x08 byte every time the player tried to delete one. Those are key
		// presses, and Key::backspace is where a text field asks about them;
		// typed() is printable text and nothing else.
		//
		// The filter is here rather than in the message translator so that it
		// holds for every feeder - a test, a replay, a second platform - and
		// not only for the one that happens to exist.
		void on_text(char32_t codepoint);

		// The window gained or lost the keyboard. Losing it clears every key,
		// which is the whole point: the key-up for anything held goes to the
		// window that took the focus and never arrives here.
		//
		// Regaining it does NOT restore what was held, because the platform
		// does not say and guessing would invent input. The both-frames rule
		// above is what makes that safe rather than merely honest.
		void set_focused(bool focused);

		// THE FRAME BOUNDARY. What was current becomes previous, what the
		// messages have been building becomes current, and the text typed
		// since the last call becomes readable. Called once per frame, before
		// anything reads it.
		void poll();

		const KeyboardState& state() const;
		const KeyboardState& previous_state() const;

		bool held(Key key) const;
		bool pressed(Key key) const;
		bool released(Key key) const;

		bool focused() const;
		bool just_focused() const;
		bool just_unfocused() const;

		// What was typed during the frame this state describes, in order, as
		// UTF-8. Empty on most frames.
		//
		// THIS IS THE CHANNEL POLLING CANNOT REBUILD, and the reason the class
		// is fed instead of read. It is not derivable from the key bits above:
		// the shift that makes `a` into `A`, the repeat that turns one held
		// key into a run of characters, the dead key that combines with the
		// next one, and the IME that turns a dozen keystrokes into one
		// ideograph are all resolved by the OS before the character arrives.
		//
		// It is a view into storage this object owns and reuses, so it is
		// valid until the next poll() and a caller that needs it longer copies
		// it. Reusing the buffer is also what keeps a frame of typing
		// allocation-free once the first one has grown it.
		std::string_view typed() const;

	private:
		// What the messages have been building since the last poll. Not
		// readable: a frame's worth of input is a snapshot, and handing out a
		// half-built one is how "down now, up last frame" stops being true.
		KeyboardState live_;

		KeyboardState current_;
		KeyboardState previous_;

		std::string typed_;
		std::string pending_text_;
	};
}
