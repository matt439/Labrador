#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace labrador
{
	// The shell feeds key and decoded-text events; poll() latches them into
	// frame snapshots. Typed characters are a separate channel because key
	// state cannot reconstruct the platform's text input.

	// Every key this engine names, one bit each in KeyboardState.
	//
	// It is a key POSITION, not a character. `Key::z` is the key a US layout
	// calls Z, whatever a French layout prints on it, because that is what a
	// movement binding wants. What was typed is typed(), which is the other
	// question and has the other answer.
	//
	// Left and right modifiers are combined: `shift`, `control` and `alt` each
	// mean either side (T3). Numpad Enter also reports as `enter`.
	enum class Key : uint8_t
	{
		// Not a key; the result of an unmapped platform code.
		none = 0,

		a, b, c, d, e, f, g, h, i, j, k, l, m,
		n, o, p, q, r, s, t, u, v, w, x, y, z,

		// Number-row positions, separate from the numpad.
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

		// Exclusive bound, not a key.
		count,
	};

	// The token for a key in a file or a console: the enumerator's own spelling
	// ("a", "digit_1", "numpad_1", "page_up", "left_bracket", "del"). Lower-case
	// ASCII, unique per key, independent of layout and platform. Empty for
	// Key::none, Key::count and any value outside the set. A name, not a label:
	// what a player is shown is the game's. Read at load, not per frame (T7).
	//
	// Saved bindings use these spellings as file-format tokens; additions
	// preserve existing tokens and their meaning.
	std::string_view key_name(Key key);

	// The key a token names. Exact and case-sensitive: no trimming, no aliases,
	// no chords. Nothing for anything else, "none" and "count" included; the
	// caller decides whether that is a default or an error, as colour_from_name
	// does.
	std::optional<Key> key_from_name(std::string_view name);

	// One frame's keyboard snapshot. Values may be constructed for edge queries.
	struct KeyboardState
	{
		// False means the window did not have the keyboard this frame. The
		// shell clears held keys on focus loss so snapshots do not retain them.
		bool focused = false;

		// One bit per Key, indexed by the enumerator; capacity is checked below.
		uint64_t keys[2] = {};

		// Tests the bit without consulting focused. False for Key::none,
		// Key::count and values outside the key set.
		bool is_down(Key key) const;
	};

	static_assert(static_cast<unsigned int>(Key::count) <= 128,
		"KeyboardState::keys is two words. A key set past 128 needs a third.");

	// Edges between two snapshots. pressed and released require focused on both
	// frames, suppressing key edges on initial focus, focus return and focus
	// loss. A hand-written snapshot must set focused to participate in edges.
	//
	// A key held when focus is lost produces neither released() nor held().
	// To stop an action on that transition, query the focus change:
	//
	//     just_unfocused(now, before) && before.is_down(Key::space)
	//
	// Edges belong to individual keys. OR-ing their pressed() values can fire
	// again while another key remains held. A game wanting one combined edge
	// compares the combined held levels across frames (T1).
	bool pressed(const KeyboardState& now, const KeyboardState& before,
		Key key);
	bool released(const KeyboardState& now, const KeyboardState& before,
		Key key);

	bool just_focused(const KeyboardState& now, const KeyboardState& before);
	bool just_unfocused(const KeyboardState& now, const KeyboardState& before);

	// Current and previous keyboard snapshots, their edges and accumulated text.
	// A game borrows Application::keyboard(); the shell owns it, feeds events and
	// polls once per frame before state updates. Games read those snapshots
	// without advancing them.
	class Keyboard
	{
	public:
		// For the shell or an input fixture. Changes become visible at poll().
		// A key pressed and released between polls has no sampled edge. Repeated
		// key-down events do not add presses; text accumulates separately.
		// Key::none, Key::count and values outside the key set are ignored.
		void on_key_down(Key key);
		void on_key_up(Key key);

		// Appends one decoded Unicode code point for the next poll(), encoded
		// as UTF-8. The platform translator assembles UTF-16 surrogate pairs.
		// Isolated surrogates and values above U+10FFFF become U+FFFD, matching
		// engine/render/text_encoding.h's replacement rule.
		//
		// C0 controls below U+0020 and U+007F are dropped. Query Enter, Tab,
		// Escape and Backspace as keys rather than appending them to text.
		void on_text(char32_t codepoint);

		// Updates focus for the next poll(). Losing focus clears held keys and
		// pending text; regaining it does not restore prior keys. Published
		// snapshots and typed() remain unchanged until poll().
		void set_focused(bool focused);

		// Called by the shell once per frame before readers. Current becomes
		// previous, fed key/focus state becomes current, and accumulated text
		// replaces the prior frame's text.
		void poll();

		// Borrowed snapshots owned by this Keyboard; poll() replaces their values.
		const KeyboardState& state() const;
		const KeyboardState& previous_state() const;

		bool held(Key key) const;
		bool pressed(Key key) const;
		bool released(Key key) const;

		bool focused() const;
		bool just_focused() const;
		bool just_unfocused() const;

		// Text published by the last poll(), in arrival order as UTF-8. The
		// returned view borrows this object's storage until the next poll() or
		// destruction; copy it to retain text longer. This is fed text, not a
		// reconstruction from key bits or a promise of platform IME support.
		std::string_view typed() const;

	private:
		// Fed state is separate from the snapshots readers observe.
		KeyboardState live_;

		KeyboardState current_;
		KeyboardState previous_;

		std::string typed_;
		std::string pending_text_;
	};
}
