#include "engine/input/keyboard.h"

namespace artattack
{
	namespace
	{
		// Key::none and Key::count are bounds rather than keys, and everything
		// below funnels through here so that neither can ever index a word.
		bool is_key(Key key)
		{
			const unsigned int index = static_cast<unsigned int>(key);
			return index > static_cast<unsigned int>(Key::none) &&
				index < static_cast<unsigned int>(Key::count);
		}

		unsigned int word_of(Key key)
		{
			return static_cast<unsigned int>(key) / 64u;
		}

		uint64_t bit_of(Key key)
		{
			return uint64_t(1) << (static_cast<unsigned int>(key) % 64u);
		}

		// U+FFFD REPLACEMENT CHARACTER, as three UTF-8 bytes.
		//
		// What an unpaired surrogate or an out-of-range code point becomes.
		// render/text_encoding.h makes the same substitution going the other
		// way, and for the same reason: a character that arrived broken should
		// be visible as broken rather than silently missing from a name the
		// player is trying to type.
		constexpr char32_t replacement_character = 0xFFFDu;

		void append_utf8(std::string& text, char32_t codepoint)
		{
			char32_t value = codepoint;

			// The surrogate range is not a character in its own right - it
			// only exists to encode astral planes in UTF-16 - so one arriving
			// here means the pairing upstream did not happen.
			if (value > 0x10FFFFu || (value >= 0xD800u && value <= 0xDFFFu))
			{
				value = replacement_character;
			}

			const auto byte = [](char32_t bits)
			{
				return static_cast<char>(static_cast<unsigned char>(bits));
			};

			if (value < 0x80u)
			{
				text.push_back(byte(value));
			}
			else if (value < 0x800u)
			{
				text.push_back(byte(0xC0u | (value >> 6)));
				text.push_back(byte(0x80u | (value & 0x3Fu)));
			}
			else if (value < 0x10000u)
			{
				text.push_back(byte(0xE0u | (value >> 12)));
				text.push_back(byte(0x80u | ((value >> 6) & 0x3Fu)));
				text.push_back(byte(0x80u | (value & 0x3Fu)));
			}
			else
			{
				text.push_back(byte(0xF0u | (value >> 18)));
				text.push_back(byte(0x80u | ((value >> 12) & 0x3Fu)));
				text.push_back(byte(0x80u | ((value >> 6) & 0x3Fu)));
				text.push_back(byte(0x80u | (value & 0x3Fu)));
			}
		}
	}

	bool KeyboardState::is_down(Key key) const
	{
		if (!is_key(key))
		{
			return false;
		}
		return (this->keys[word_of(key)] & bit_of(key)) != 0;
	}

	bool pressed(const KeyboardState& now, const KeyboardState& before,
		Key key)
	{
		// Both frames, for the three reasons keyboard.h lists. Without it the
		// first frame after launch, and every frame after an alt-tab back,
		// reports a press for a key nobody touched.
		if (!now.focused || !before.focused)
		{
			return false;
		}
		return now.is_down(key) && !before.is_down(key);
	}

	bool released(const KeyboardState& now, const KeyboardState& before,
		Key key)
	{
		if (!now.focused || !before.focused)
		{
			return false;
		}
		return !now.is_down(key) && before.is_down(key);
	}

	bool just_focused(const KeyboardState& now, const KeyboardState& before)
	{
		return now.focused && !before.focused;
	}

	bool just_unfocused(const KeyboardState& now, const KeyboardState& before)
	{
		return !now.focused && before.focused;
	}

	void Keyboard::on_key_down(Key key)
	{
		if (!is_key(key))
		{
			return;
		}

		// Idempotent, which is what makes key repeat cost nothing. Windows
		// resends WM_KEYDOWN while a key is held; setting a bit that is
		// already set changes nothing, and because the edges below are derived
		// from two frames rather than from the messages, a repeat cannot
		// manufacture a second pressed(). The poll design gets that for free -
		// a message-driven one would have to filter on the repeat flag.
		this->live_.keys[word_of(key)] |= bit_of(key);
	}

	void Keyboard::on_key_up(Key key)
	{
		if (!is_key(key))
		{
			return;
		}
		this->live_.keys[word_of(key)] &= ~bit_of(key);
	}

	void Keyboard::on_text(char32_t codepoint)
	{
		// The C0 controls and DEL are keys, not text. Enter, Tab, Escape and
		// Backspace all arrive as characters as well as key codes, and a field
		// that appended them would collect a backspace byte every time
		// somebody tried to remove one.
		if (codepoint < 0x20u || codepoint == 0x7Fu)
		{
			return;
		}

		// Accumulated rather than replaced, because a frame is long enough to
		// hold several characters: key repeat alone produces a run of them,
		// and an IME can commit a whole word at once. Sampling this the way
		// the key bits are sampled would keep the last one and drop the rest.
		append_utf8(this->pending_text_, codepoint);
	}

	void Keyboard::set_focused(bool focused)
	{
		this->live_.focused = focused;

		if (!focused)
		{
			// EVERY KEY GOES UP, and this is the line the flag exists for. The
			// key-up for anything held while the window loses the foreground is
			// delivered to whatever gained it, so a bit left set here is set
			// until the player happens to press and release that key again.
			this->live_.keys[0] = 0;
			this->live_.keys[1] = 0;

			// Half-typed text goes with it. It was typed into this window and
			// the window is gone; delivering it on the frame focus returns
			// would put it in whatever field is open then.
			this->pending_text_.clear();
		}
	}

	void Keyboard::poll()
	{
		this->previous_ = this->current_;
		this->current_ = this->live_;

		// swap rather than assign: the buffer typed_ gives up becomes the one
		// pending_text_ accumulates into, so a game where somebody is typing
		// stops allocating once both have grown once.
		this->typed_.swap(this->pending_text_);
		this->pending_text_.clear();
	}

	const KeyboardState& Keyboard::state() const
	{
		return this->current_;
	}

	const KeyboardState& Keyboard::previous_state() const
	{
		return this->previous_;
	}

	bool Keyboard::held(Key key) const
	{
		return this->current_.focused && this->current_.is_down(key);
	}

	bool Keyboard::pressed(Key key) const
	{
		return artattack::pressed(this->current_, this->previous_, key);
	}

	bool Keyboard::released(Key key) const
	{
		return artattack::released(this->current_, this->previous_, key);
	}

	bool Keyboard::focused() const
	{
		return this->current_.focused;
	}

	bool Keyboard::just_focused() const
	{
		return artattack::just_focused(this->current_, this->previous_);
	}

	bool Keyboard::just_unfocused() const
	{
		return artattack::just_unfocused(this->current_, this->previous_);
	}

	std::string_view Keyboard::typed() const
	{
		return this->typed_;
	}
}
