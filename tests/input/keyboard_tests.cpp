#include <doctest/doctest.h>

#include "engine/input/keyboard.h"

#include <array>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <optional>
// Not a stray include. doctest only forward-declares std::basic_ostream, and
// reporting a failed comparison of std::string_view calls the standard
// library's operator<< for one, which needs the stream type complete. Any
// test file whose assertions compare a std::string_view needs it.
#include <ostream>
#include <set>
#include <string>
#include <string_view>

using namespace labrador;

namespace
{
	// A keyboard with the named keys down and nothing else, focused. Two of
	// these are a frame boundary, which is all an edge is - and writing them
	// by hand is why the edge logic lives in free functions rather than behind
	// the window.
	KeyboardState board(std::initializer_list<Key> down)
	{
		KeyboardState state;
		state.focused = true;
		for (const Key key : down)
		{
			const unsigned int index = static_cast<unsigned int>(key);
			state.keys[index / 64u] |= uint64_t(1) << (index % 64u);
		}
		return state;
	}

	// The same, with the window not focused.
	KeyboardState unfocused(std::initializer_list<Key> down)
	{
		KeyboardState state = board(down);
		state.focused = false;
		return state;
	}

	// Expected UTF-8, spelt a byte at a time. A string literal would need hex
	// escapes, and those are greedy - "\xC3" followed by a letter swallows it.
	std::string utf8(std::initializer_list<int> bytes)
	{
		std::string result;
		for (const int value : bytes)
		{
			result.push_back(static_cast<char>(
				static_cast<unsigned char>(value)));
		}
		return result;
	}

	// A Keyboard that has been focused for two frames, so edges are live.
	// Without the second poll every edge is suppressed by the both-frames
	// rule, which is correct and is itself a test below.
	void settle(Keyboard& keyboard)
	{
		keyboard.set_focused(true);
		keyboard.poll();
		keyboard.poll();
	}

	std::string_view required_label(const Keyboard& keyboard, Key key)
	{
		const std::optional<std::string_view> value = keyboard.label(key);
		REQUIRE(value.has_value());
		return value.value_or(std::string_view());
	}
}

TEST_SUITE("KeyboardTests")
{
	TEST_CASE("a key is down or it is not")
	{
		const KeyboardState state = board({ Key::a, Key::left });

		CHECK(state.is_down(Key::a));
		CHECK(state.is_down(Key::left));
		CHECK_FALSE(state.is_down(Key::b));
		CHECK_FALSE(state.is_down(Key::right));
	}

	TEST_CASE("the bounds are not keys")
	{
		// Key::none is what an unmapped platform code translates to, so it
		// is asked this question by anything that forwards a translation
		// straight in. Both answers have to be no rather than a read of
		// whatever bit they would index - so every bit is set, and only
		// the bounds check can answer no.
		KeyboardState state;
		state.focused = true;
		state.keys[0] = ~uint64_t(0);
		state.keys[1] = ~uint64_t(0);

		CHECK(state.is_down(Key::a));
		CHECK_FALSE(state.is_down(Key::none));
		CHECK_FALSE(state.is_down(Key::count));
	}

	TEST_CASE("every key gets its own bit")
	{
		// The set spans two words, so a shift that forgot to divide - or
		// one that overflowed a single uint64_t - would alias two keys
		// onto one. This is the check that the split into keys[0] and
		// keys[1] is done consistently everywhere.
		const unsigned int first = static_cast<unsigned int>(Key::none) + 1;
		const unsigned int last = static_cast<unsigned int>(Key::count);

		for (unsigned int i = first; i < last; ++i)
		{
			const Key key = static_cast<Key>(i);
			const KeyboardState state = board({ key });

			CHECK(state.is_down(key));

			for (unsigned int j = first; j < last; ++j)
			{
				if (i != j)
				{
					CHECK_FALSE(state.is_down(static_cast<Key>(j)));
				}
			}
		}
	}

	TEST_CASE("an edge is down now and up last frame")
	{
		const KeyboardState before = board({});
		const KeyboardState now = board({ Key::space });

		CHECK(pressed(now, before, Key::space));
		CHECK_FALSE(released(now, before, Key::space));

		CHECK(released(before, now, Key::space));
		CHECK_FALSE(pressed(before, now, Key::space));
	}

	TEST_CASE("holding a key is not pressing it again")
	{
		// Key repeat resends the message while a key is held. The edges
		// are derived from two frames rather than from the messages, so a
		// repeat cannot manufacture a second press - this is the assertion
		// that keeps that true.
		const KeyboardState before = board({ Key::a });
		const KeyboardState now = board({ Key::a });

		CHECK_FALSE(pressed(now, before, Key::a));
		CHECK_FALSE(released(now, before, Key::a));
	}

	TEST_CASE("an edge requires focus on both frames")
	{
		// The three doors keyboard.h names, one per section below.

		SUBCASE("the first frame of the process")
		{
			// A default-constructed previous is unfocused, so a key
			// already held at launch is not a press.
			const KeyboardState before;
			const KeyboardState now = board({ Key::a });

			CHECK_FALSE(pressed(now, before, Key::a));
		}

		SUBCASE("focus returning with a key already down")
		{
			const KeyboardState before = unfocused({});
			const KeyboardState now = board({ Key::a });

			CHECK_FALSE(pressed(now, before, Key::a));
		}

		SUBCASE("focus leaving with a chord held")
		{
			// Losing focus clears every key at once. Without the rule the
			// whole chord fires released() on that one frame.
			const KeyboardState before =
				board({ Key::a, Key::shift, Key::space });
			const KeyboardState now = unfocused({});

			CHECK_FALSE(released(now, before, Key::a));
			CHECK_FALSE(released(now, before, Key::shift));
			CHECK_FALSE(released(now, before, Key::space));
		}
	}

	TEST_CASE("the focus change is what a release-keyed client asks for")
	{
		// The spelling keyboard.h documents for the case the rule above
		// takes away.
		const KeyboardState before = board({ Key::space });
		const KeyboardState now = unfocused({});

		CHECK(just_unfocused(now, before));
		CHECK(before.is_down(Key::space));

		CHECK_FALSE(just_focused(now, before));
		CHECK(just_focused(before, now));
	}

	TEST_CASE("nothing is visible until the frame turns over")
	{
		Keyboard keyboard;
		settle(keyboard);

		keyboard.on_key_down(Key::a);
		CHECK_FALSE(keyboard.held(Key::a));

		keyboard.poll();
		CHECK(keyboard.held(Key::a));
		CHECK(keyboard.pressed(Key::a));

		keyboard.poll();
		CHECK(keyboard.held(Key::a));
		CHECK_FALSE(keyboard.pressed(Key::a));

		keyboard.on_key_up(Key::a);
		keyboard.poll();
		CHECK_FALSE(keyboard.held(Key::a));
		CHECK(keyboard.released(Key::a));
	}

	TEST_CASE("losing focus puts every key up")
	{
		Keyboard keyboard;
		settle(keyboard);

		keyboard.on_key_down(Key::a);
		keyboard.on_key_down(Key::shift);
		keyboard.poll();
		CHECK(keyboard.held(Key::a));

		// The key-up for both of these is delivered to whatever took the
		// foreground and never arrives here. A device that waited for it
		// would hold them forever.
		keyboard.set_focused(false);
		keyboard.poll();

		CHECK_FALSE(keyboard.held(Key::a));
		CHECK_FALSE(keyboard.held(Key::shift));
		CHECK_FALSE(keyboard.focused());
		CHECK(keyboard.just_unfocused());

		// And no phantom release, per the both-frames rule.
		CHECK_FALSE(keyboard.released(Key::a));
	}

	TEST_CASE("focus returning does not press what was held")
	{
		Keyboard keyboard;
		settle(keyboard);

		keyboard.on_key_down(Key::a);
		keyboard.poll();
		keyboard.set_focused(false);
		keyboard.poll();

		keyboard.set_focused(true);
		keyboard.poll();
		CHECK(keyboard.just_focused());
		CHECK_FALSE(keyboard.pressed(Key::a));

		// A real press after the window is back reads normally again.
		keyboard.on_key_down(Key::a);
		keyboard.poll();
		CHECK(keyboard.pressed(Key::a));
	}

	TEST_CASE("typed text accumulates within a frame and clears after it")
	{
		Keyboard keyboard;
		settle(keyboard);

		CHECK(keyboard.typed().empty());

		// A fast typist genuinely produces several characters inside one
		// frame, and key repeat produces a run of them. Sampling the way
		// the key bits are sampled would keep the last and drop the rest.
		keyboard.on_text(U'a');
		keyboard.on_text(U'b');
		keyboard.on_text(U'c');
		CHECK(keyboard.typed().empty());

		keyboard.poll();
		CHECK(keyboard.typed() == "abc");

		keyboard.poll();
		CHECK(keyboard.typed().empty());
	}

	TEST_CASE("typed text is UTF-8, whatever plane it came from")
	{
		Keyboard keyboard;
		settle(keyboard);

		SUBCASE("one byte")
		{
			keyboard.on_text(U'A');
			keyboard.poll();
			CHECK(keyboard.typed() == utf8({ 0x41 }));
		}

		SUBCASE("two bytes")
		{
			// U+00E9, LATIN SMALL LETTER E WITH ACUTE. Spelt as a number
			// rather than as itself so that this file stays pure ASCII -
			// a source character above 127 needs the compiler told which
			// encoding the file is in, and being wrong about that is a
			// silent corruption rather than an error.
			keyboard.on_text(static_cast<char32_t>(0x00E9u));
			keyboard.poll();
			CHECK(keyboard.typed() == utf8({ 0xC3, 0xA9 }));
		}

		SUBCASE("three bytes")
		{
			// U+20AC, EURO SIGN.
			keyboard.on_text(static_cast<char32_t>(0x20ACu));
			keyboard.poll();
			CHECK(keyboard.typed() == utf8({ 0xE2, 0x82, 0xAC }));
		}

		SUBCASE("four bytes")
		{
			// Past the basic plane, which is where the window has already
			// had to assemble a surrogate pair to produce one of these.
			keyboard.on_text(U'\U0001F600');
			keyboard.poll();
			CHECK(keyboard.typed() == utf8({ 0xF0, 0x9F, 0x98, 0x80 }));
		}
	}

	TEST_CASE("a broken code point becomes U+FFFD rather than nothing")
	{
		// render/text_encoding.h's rule, going the other way: text about
		// to be drawn should show mojibake, not vanish.
		const std::string replacement = utf8({ 0xEF, 0xBF, 0xBD });

		SUBCASE("an unpaired surrogate")
		{
			Keyboard keyboard;
			settle(keyboard);
			keyboard.on_text(static_cast<char32_t>(0xD800u));
			keyboard.poll();
			CHECK(keyboard.typed() == replacement);
		}

		SUBCASE("past the last code point there is")
		{
			Keyboard keyboard;
			settle(keyboard);
			keyboard.on_text(static_cast<char32_t>(0x110000u));
			keyboard.poll();
			CHECK(keyboard.typed() == replacement);
		}
	}

	TEST_CASE("control characters are keys, not text")
	{
		// Enter, Tab, Escape and Backspace all arrive as characters as
		// well as key codes. A name field that appended them would collect
		// a backspace byte every time somebody tried to remove one.
		Keyboard keyboard;
		settle(keyboard);

		keyboard.on_text(U'\b');
		keyboard.on_text(U'\t');
		keyboard.on_text(U'\r');
		keyboard.on_text(U'\n');
		keyboard.on_text(static_cast<char32_t>(0x1Bu));
		keyboard.on_text(static_cast<char32_t>(0x7Fu));
		keyboard.on_text(U'k');
		keyboard.poll();

		CHECK(keyboard.typed() == "k");
	}

	TEST_CASE("losing focus drops text typed but not yet delivered")
	{
		// It was typed at this window. Delivering it on the frame focus
		// returns would put it in whatever field is open then.
		Keyboard keyboard;
		settle(keyboard);

		keyboard.on_text(U'h');
		keyboard.on_text(U'i');
		keyboard.set_focused(false);
		keyboard.poll();

		CHECK(keyboard.typed().empty());
	}

	TEST_CASE("an unmapped key changes nothing")
	{
		// What a keyboard this engine has never heard of translates to.
		// The device drops it rather than indexing a bit that means
		// something else.
		Keyboard keyboard;
		settle(keyboard);

		keyboard.on_key_down(Key::none);
		keyboard.on_key_down(Key::count);
		keyboard.poll();

		const KeyboardState& state = keyboard.state();
		CHECK(state.keys[0] == 0);
		CHECK(state.keys[1] == 0);
	}

	TEST_CASE("every key has a name of its own, made of [a-z0-9_]")
	{
		// The round trip below is what holds the set complete on MSVC,
		// where a switch missing a case is not an error: a key added to
		// the enum without a name fails here.
		std::set<std::string_view> seen;
		for (unsigned int index = static_cast<unsigned int>(Key::a);
			index < static_cast<unsigned int>(Key::count); ++index)
		{
			const Key key = static_cast<Key>(index);
			const std::string_view name = key_name(key);
			CAPTURE(index);
			REQUIRE_FALSE(name.empty());
			for (const char character : name)
			{
				const bool allowed = (character >= 'a' && character <= 'z') ||
					(character >= '0' && character <= '9') ||
					character == '_';
				CHECK(allowed);
			}
			CHECK(seen.insert(name).second);
		}
		CHECK(seen.size() ==
			static_cast<std::size_t>(Key::count) -
			static_cast<std::size_t>(Key::a));
	}

	TEST_CASE("a name reads back as the key it names, and only that")
	{
		for (unsigned int index = static_cast<unsigned int>(Key::a);
			index < static_cast<unsigned int>(Key::count); ++index)
		{
			const Key key = static_cast<Key>(index);
			const std::string_view name = key_name(key);
			CAPTURE(index);
			const std::optional<Key> read = key_from_name(name);
			REQUIRE(read.has_value());
			CHECK(*read == key);
			CHECK(key_name(*read) == name);
		}
	}

	TEST_CASE("the bounds and anything outside the set have no name")
	{
		CHECK(key_name(Key::none).empty());
		CHECK(key_name(Key::count).empty());
		CHECK(key_name(static_cast<Key>(255)).empty());
	}

	TEST_CASE("a name is exact: no case folding, trimming, aliases or chords")
	{
		const std::string_view refused[] = {
			"", "none", "count", "J", "Space", " j", "j ", "ctrl+j",
			"KeyJ", "delete", std::string_view("j\0", 2),
		};
		for (const std::string_view name : refused)
		{
			CAPTURE(std::string(name));
			CHECK_FALSE(key_from_name(name).has_value());
		}
	}

	TEST_CASE("the spellings a reader would guess wrong are the enumerators'")
	{
		CHECK(key_name(Key::digit_1) == "digit_1");
		CHECK(key_name(Key::numpad_1) == "numpad_1");
		CHECK(key_name(Key::del) == "del");
		CHECK(key_name(Key::page_up) == "page_up");
		CHECK(key_name(Key::left_bracket) == "left_bracket");
		CHECK(key_name(Key::grave) == "grave");
		CHECK(key_from_name("del") == Key::del);
		CHECK(key_from_name("digit_1") == Key::digit_1);
		CHECK(key_from_name("numpad_1") == Key::numpad_1);
	}
}

TEST_CASE("existing key tokens retain their published numeric order")
{
	// Freeze the published tokens and key values independently of the current
	// enum, so inserting or renaming a value cannot change both sides of a test.
	constexpr std::string_view original_names[] = {
		"a", "b", "c", "d", "e",
		"f", "g", "h", "i", "j",
		"k", "l", "m", "n", "o",
		"p", "q", "r", "s", "t",
		"u", "v", "w", "x", "y",
		"z", "digit_0", "digit_1", "digit_2", "digit_3",
		"digit_4", "digit_5", "digit_6", "digit_7", "digit_8",
		"digit_9", "f1", "f2", "f3", "f4",
		"f5", "f6", "f7", "f8", "f9",
		"f10", "f11", "f12", "escape", "tab",
		"caps_lock", "shift", "control", "alt", "space",
		"enter", "backspace", "insert", "del", "home",
		"end", "page_up", "page_down", "left", "right",
		"up", "down", "print_screen", "scroll_lock", "pause",
		"num_lock", "minus", "equals", "left_bracket", "right_bracket",
		"backslash", "semicolon", "apostrophe", "grave", "comma",
		"period", "slash", "numpad_0", "numpad_1", "numpad_2",
		"numpad_3", "numpad_4", "numpad_5", "numpad_6", "numpad_7",
		"numpad_8", "numpad_9", "numpad_add", "numpad_subtract", "numpad_multiply",
		"numpad_divide", "numpad_decimal",
	};
	for (std::size_t index = 0; index < std::size(original_names); ++index)
	{
		const Key key = static_cast<Key>(index + 1);
		CAPTURE(index);
		CHECK(key_name(key) == original_names[index]);
		const std::optional<Key> read = key_from_name(original_names[index]);
		REQUIRE(read.has_value());
		CHECK(*read == key);
	}
	CHECK(static_cast<std::size_t>(Key::international_backslash) == std::size(original_names) + 1);
	CHECK(key_name(Key::international_backslash) == "international_backslash");
	CHECK(key_from_name("international_backslash") == Key::international_backslash);
	CHECK_FALSE(key_from_name("IntlBackslash").has_value());
}

TEST_CASE("the ISO extra key has independent sampled edges and focus cancellation")
{
	Keyboard keyboard;
	keyboard.set_focused(true);
	keyboard.poll();
	keyboard.poll();
	keyboard.on_key_down(Key::international_backslash);
	keyboard.on_key_down(Key::backslash);
	keyboard.poll();
	CHECK(keyboard.pressed(Key::international_backslash));
	CHECK(keyboard.pressed(Key::backslash));
	keyboard.on_key_down(Key::international_backslash);
	keyboard.poll();
	CHECK_FALSE(keyboard.pressed(Key::international_backslash));
	keyboard.on_key_up(Key::international_backslash);
	keyboard.poll();
	CHECK(keyboard.released(Key::international_backslash));
	CHECK(keyboard.held(Key::backslash));
	keyboard.on_key_down(Key::international_backslash);
	keyboard.poll();
	keyboard.set_focused(false);
	keyboard.poll();
	CHECK_FALSE(keyboard.held(Key::international_backslash));
	CHECK_FALSE(keyboard.released(Key::international_backslash));
	CHECK_FALSE(keyboard.held(Key::backslash));
	CHECK_FALSE(keyboard.released(Key::backslash));
	keyboard.set_focused(true);
	keyboard.poll();
	CHECK_FALSE(keyboard.pressed(Key::international_backslash));
	keyboard.on_key_down(Key::none);
	keyboard.on_key_down(static_cast<Key>(255));
	keyboard.poll();
	CHECK_FALSE(keyboard.held(Key::international_backslash));
	CHECK_FALSE(keyboard.held(Key::backslash));
}

TEST_SUITE("KeyboardLabelTests")
{
	TEST_CASE("unseen and invalid key labels are unavailable without inventing token labels")
	{
		Keyboard keyboard;
		CHECK(keyboard.label_revision() == 0);
		for (unsigned int index = 0; index <= 255; ++index)
		{
			CHECK_FALSE(keyboard.label(static_cast<Key>(index)).has_value());
		}
		for (Key key : { Key::none, Key::count, static_cast<Key>(255) })
		{
			keyboard.on_key_label(key, "Unavailable position");
		}
		keyboard.poll();
		CHECK(keyboard.label_revision() == 0);
		CHECK_FALSE(keyboard.label(Key::a).has_value());
		CHECK_FALSE(keyboard.label(Key::none).has_value());
		CHECK_FALSE(keyboard.label(Key::count).has_value());
		CHECK_FALSE(keyboard.label(static_cast<Key>(255)).has_value());
	}

	TEST_CASE("key labels publish with the next input snapshot without remapping its identity")
	{
		Keyboard keyboard;
		settle(keyboard);
		keyboard.on_key_label(Key::z, "W");
		keyboard.on_key_down(Key::z);
		keyboard.on_text(U'w');
		CHECK_FALSE(keyboard.label(Key::z).has_value());
		CHECK_FALSE(keyboard.held(Key::z));
		CHECK(keyboard.typed().empty());
		CHECK(keyboard.label_revision() == 0);
		keyboard.poll();
		REQUIRE(keyboard.label(Key::z).has_value());
		CHECK(required_label(keyboard, Key::z) == "W");
		CHECK(keyboard.held(Key::z));
		CHECK_FALSE(keyboard.held(Key::w));
		CHECK(keyboard.typed() == "w");
		CHECK(keyboard.label_revision() == 1);
		CHECK(key_name(Key::z) == "z");
		CHECK(key_from_name("z") == Key::z);
	}

	TEST_CASE("label feeds copy caller storage while published views survive staged changes")
	{
		Keyboard keyboard;
		std::string supplied = "A long platform label owned by a temporary shell buffer";
		const std::string expected = supplied;
		keyboard.on_key_label(Key::a, supplied);
		supplied.assign("Changed by the shell before poll");
		keyboard.poll();
		const std::optional<std::string_view> published = keyboard.label(Key::a);
		REQUIRE(published.has_value());
		CHECK(*published == expected);
		keyboard.on_key_label(Key::a, "A replacement label with a different allocation length");
		CHECK(*published == expected);
		CHECK(required_label(keyboard, Key::a) == expected);
		keyboard.on_key_labels_invalidated();
		CHECK(*published == expected);
		CHECK(required_label(keyboard, Key::a) == expected);
		keyboard.on_key_label(Key::a, *published);
		CHECK(*published == expected);
		keyboard.poll();
		// The borrowed pre-poll view is no longer used after this boundary.
		CHECK(required_label(keyboard, Key::a) == expected);
		CHECK(keyboard.label_revision() == 1);
	}

	TEST_CASE("one label publication advances revision once and unchanged facts do not advance it")
	{
		Keyboard keyboard;
		keyboard.on_key_label(Key::a, "A");
		keyboard.on_key_label(Key::b, "B");
		keyboard.poll();
		CHECK(keyboard.label_revision() == 1);
		keyboard.poll();
		CHECK(keyboard.label_revision() == 1);
		keyboard.on_key_label(Key::a, "A");
		keyboard.on_key_label(Key::b, "B");
		keyboard.on_key_label(Key::c, "");
		keyboard.poll();
		CHECK(keyboard.label_revision() == 1);
		keyboard.on_key_label(Key::a, "Transient observation");
		keyboard.on_key_label(Key::a, "A");
		keyboard.poll();
		CHECK(keyboard.label_revision() == 1);
		keyboard.on_key_label(Key::a, "Q");
		keyboard.on_key_label(Key::b, "W");
		CHECK(keyboard.label_revision() == 1);
		keyboard.poll();
		CHECK(keyboard.label_revision() == 2);
		CHECK(required_label(keyboard, Key::a) == "Q");
		CHECK(required_label(keyboard, Key::b) == "W");
	}

	TEST_CASE("label deltas preserve unrelated facts across successive publications")
	{
		Keyboard keyboard;
		keyboard.on_key_label(Key::a, "A");
		keyboard.on_key_label(Key::b, "B");
		keyboard.poll();
		keyboard.on_key_label(Key::a, "Q");
		keyboard.poll();
		CHECK(required_label(keyboard, Key::a) == "Q");
		CHECK(required_label(keyboard, Key::b) == "B");
		keyboard.on_key_label(Key::b, "W");
		keyboard.poll();
		CHECK(required_label(keyboard, Key::a) == "Q");
		CHECK(required_label(keyboard, Key::b) == "W");
		CHECK(keyboard.label_revision() == 3);
		keyboard.on_key_label(Key::a, "");
		CHECK(keyboard.label(Key::a).has_value());
		keyboard.poll();
		CHECK_FALSE(keyboard.label(Key::a).has_value());
		CHECK(required_label(keyboard, Key::b) == "W");
		CHECK(keyboard.label_revision() == 4);
		keyboard.on_key_label(Key::a, std::string_view());
		keyboard.poll();
		CHECK(keyboard.label_revision() == 4);
	}

	TEST_CASE("invalidation clears only at poll and republishing the same complete map is invisible")
	{
		Keyboard keyboard;
		keyboard.on_key_label(Key::a, "Q");
		keyboard.on_key_label(Key::b, "W");
		keyboard.poll();
		keyboard.on_key_labels_invalidated();
		keyboard.on_key_label(Key::b, "W");
		keyboard.on_key_label(Key::a, "Q");
		keyboard.poll();
		CHECK(keyboard.label_revision() == 1);
		CHECK(required_label(keyboard, Key::a) == "Q");
		CHECK(required_label(keyboard, Key::b) == "W");
		keyboard.on_key_labels_invalidated();
		keyboard.on_key_label(Key::a, "Q");
		CHECK(required_label(keyboard, Key::b) == "W");
		keyboard.poll();
		CHECK(required_label(keyboard, Key::a) == "Q");
		CHECK_FALSE(keyboard.label(Key::b).has_value());
		CHECK(keyboard.label_revision() == 2);
		keyboard.on_key_labels_invalidated();
		CHECK(keyboard.label(Key::a).has_value());
		keyboard.poll();
		CHECK_FALSE(keyboard.label(Key::a).has_value());
		CHECK(keyboard.label_revision() == 3);
		keyboard.on_key_labels_invalidated();
		keyboard.poll();
		CHECK(keyboard.label_revision() == 3);
	}

	TEST_CASE("focus does not discard label facts and the shell explicitly expires them")
	{
		Keyboard keyboard;
		keyboard.on_key_label(Key::a, "Q");
		keyboard.poll();
		keyboard.set_focused(true);
		keyboard.poll();
		CHECK(required_label(keyboard, Key::a) == "Q");
		CHECK(keyboard.label_revision() == 1);
		keyboard.set_focused(false);
		keyboard.poll();
		CHECK(required_label(keyboard, Key::a) == "Q");
		CHECK(keyboard.label_revision() == 1);
		keyboard.on_key_label(Key::a, "A");
		keyboard.poll();
		CHECK_FALSE(keyboard.focused());
		CHECK(required_label(keyboard, Key::a) == "A");
		CHECK(keyboard.label_revision() == 2);
		keyboard.on_key_labels_invalidated();
		keyboard.poll();
		keyboard.set_focused(true);
		keyboard.poll();
		CHECK_FALSE(keyboard.label(Key::a).has_value());
		CHECK(keyboard.label_revision() == 3);
	}

	TEST_CASE("UTF-8 label bytes are preserved independently of font support or saved key tokens")
	{
		Keyboard keyboard;
		const std::array<std::string, 4> labels{
			utf8({ 0xC3, 0xA9 }), utf8({ 0xE4, 0xB8, 0xAD }),
			utf8({ 0xF0, 0x9F, 0x98, 0x80 }), utf8({ 0x65, 0xCC, 0x81 })
		};
		uint64_t revision = 0;
		for (const std::string& label : labels)
		{
			keyboard.on_key_label(Key::international_backslash, label);
			keyboard.poll();
			++revision;
			REQUIRE(keyboard.label(Key::international_backslash).has_value());
			CHECK(required_label(keyboard, Key::international_backslash) == label);
			CHECK(keyboard.label_revision() == revision);
			CHECK(key_name(Key::international_backslash) == "international_backslash");
			CHECK_FALSE(key_from_name(label).has_value());
		}
	}
}
