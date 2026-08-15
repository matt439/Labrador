// BEFORE doctest, and it is not a stray include. doctest forward-declares
// std::basic_ostream rather than including anything, which is fine until an
// assertion compares a std::string_view: reporting the failure would call the
// standard library's operator<< for one, and that needs the stream type
// complete. This is the only test in the tree that compares strings, so this
// is the only file that has met it.
#include <ostream>

#include <doctest/doctest.h>
#include "engine/input/keyboard.h"

#include <initializer_list>
#include <string>

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
}

namespace KeyboardTests
{
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
			// whatever bit they would index.
			const KeyboardState state = board({ Key::a });

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
	}
}
