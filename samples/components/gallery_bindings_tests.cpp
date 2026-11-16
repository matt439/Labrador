#include <doctest/doctest.h>

#include "samples/components/gallery_bindings.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <string_view>

using components::BindingResult;
using components::GalleryAction;
using components::GalleryBindings;
using labrador::Key;

namespace
{
	constexpr std::array<Key, 36> rebindable_keys{
		Key::a, Key::b, Key::c, Key::d, Key::e, Key::f, Key::g,
		Key::h, Key::i, Key::j, Key::k, Key::l, Key::m, Key::n,
		Key::o, Key::p, Key::q, Key::r, Key::s, Key::t, Key::u,
		Key::v, Key::w, Key::x, Key::y, Key::z,
		Key::digit_0, Key::digit_1, Key::digit_2, Key::digit_3, Key::digit_4,
		Key::digit_5, Key::digit_6, Key::digit_7, Key::digit_8, Key::digit_9
	};
}

TEST_CASE("gallery bindings start with the two stable default tokens")
{
	const GalleryBindings bindings;
	CHECK(bindings.key(GalleryAction::restart_previews) == Key::r);
	CHECK(bindings.key(GalleryAction::reset_difficulty) == Key::d);
	CHECK(bindings.tokens() == std::array<std::string_view, 2>{ "r", "d" });
}

TEST_CASE("gallery bindings change one action and keep unchanged assignments explicit")
{
	GalleryBindings bindings;
	CHECK(bindings.try_bind(GalleryAction::restart_previews, Key::r) == BindingResult::unchanged);
	CHECK(bindings.try_bind(GalleryAction::restart_previews, Key::digit_7) == BindingResult::changed);
	CHECK(bindings.key(GalleryAction::restart_previews) == Key::digit_7);
	CHECK(bindings.key(GalleryAction::reset_difficulty) == Key::d);
	CHECK(bindings.tokens() == std::array<std::string_view, 2>{ "digit_7", "d" });
	CHECK(bindings.try_bind(GalleryAction::restart_previews, Key::digit_7) == BindingResult::unchanged);
}

TEST_CASE("a duplicate gallery key leaves both actions unchanged")
{
	GalleryBindings bindings;
	const std::array<std::string_view, 2> before = bindings.tokens();
	CHECK(bindings.try_bind(GalleryAction::restart_previews, Key::d) == BindingResult::duplicate);
	CHECK(bindings.tokens() == before);
	CHECK(bindings.try_bind(GalleryAction::reset_difficulty, Key::r) == BindingResult::duplicate);
	CHECK(bindings.tokens() == before);
	CHECK(bindings.key(GalleryAction::restart_previews) == Key::r);
	CHECK(bindings.key(GalleryAction::reset_difficulty) == Key::d);
}

TEST_CASE("gallery shortcuts accept every letter and number-row position")
{
	for (Key candidate : rebindable_keys)
	{
		GalleryBindings bindings;
		if (candidate == Key::d)
		{
			CHECK(bindings.try_bind(GalleryAction::reset_difficulty, Key::x) == BindingResult::changed);
		}
		const BindingResult expected = candidate == Key::r ? BindingResult::unchanged : BindingResult::changed;
		CHECK(bindings.try_bind(GalleryAction::restart_previews, candidate) == expected);
		CHECK(bindings.key(GalleryAction::restart_previews) == candidate);
	}
}

TEST_CASE("all other gallery keys remain protected including invalid enum values")
{
	for (unsigned int value = 0; value <= UINT8_MAX; ++value)
	{
		const Key candidate = static_cast<Key>(value);
		if (std::find(rebindable_keys.begin(), rebindable_keys.end(), candidate) != rebindable_keys.end()) continue;
		GalleryBindings bindings;
		const std::array<std::string_view, 2> before = bindings.tokens();
		CHECK(bindings.try_bind(GalleryAction::restart_previews, candidate) == BindingResult::protected_key);
		CHECK(bindings.try_bind(GalleryAction::reset_difficulty, candidate) == BindingResult::protected_key);
		CHECK(bindings.tokens() == before);
	}
}

TEST_CASE("gallery binding tokens round trip and a complete pair can exchange keys")
{
	GalleryBindings bindings;
	CHECK(bindings.load_tokens({ "d", "r" }) == BindingResult::changed);
	CHECK(bindings.key(GalleryAction::restart_previews) == Key::d);
	CHECK(bindings.key(GalleryAction::reset_difficulty) == Key::r);
	CHECK(bindings.load_tokens({ "digit_0", "z" }) == BindingResult::changed);
	const std::array<std::string_view, 2> saved = bindings.tokens();
	CHECK(saved == std::array<std::string_view, 2>{ "digit_0", "z" });
	GalleryBindings restored;
	CHECK(restored.load_tokens(saved) == BindingResult::changed);
	CHECK(restored.tokens() == saved);
	CHECK(restored.load_tokens(restored.tokens()) == BindingResult::unchanged);
}

TEST_CASE("invalid gallery tokens refuse the entire loaded table without trimming or aliases")
{
	const std::array<std::string_view, 12> invalid{
		"", "R", "Digit_1", "1", "Key::r", "r ", " r", "none", "count", "shift+r", "unknown",
		std::string_view("r\0junk", 6)
	};
	GalleryBindings bindings;
	REQUIRE(bindings.load_tokens({ "x", "digit_3" }) == BindingResult::changed);
	const std::array<std::string_view, 2> before = bindings.tokens();
	for (std::string_view token : invalid)
	{
		CHECK(bindings.load_tokens({ "a", token }) == BindingResult::invalid_token);
		CHECK(bindings.tokens() == before);
		CHECK(bindings.load_tokens({ token, "b" }) == BindingResult::invalid_token);
		CHECK(bindings.tokens() == before);
	}
}

TEST_CASE("protected and duplicate loaded gallery tokens leave the old table intact")
{
	GalleryBindings bindings;
	const std::array<std::string_view, 2> before = bindings.tokens();
	CHECK(bindings.load_tokens({ "a", "left" }) == BindingResult::protected_key);
	CHECK(bindings.tokens() == before);
	CHECK(bindings.load_tokens({ "enter", "b" }) == BindingResult::protected_key);
	CHECK(bindings.tokens() == before);
	CHECK(bindings.load_tokens({ "q", "q" }) == BindingResult::duplicate);
	CHECK(bindings.tokens() == before);
}

TEST_CASE("reset restores both gallery defaults after a custom table")
{
	GalleryBindings bindings;
	REQUIRE(bindings.load_tokens({ "z", "digit_9" }) == BindingResult::changed);
	bindings.reset();
	CHECK(bindings.tokens() == std::array<std::string_view, 2>{ "r", "d" });
	bindings.reset();
	CHECK(bindings.key(GalleryAction::restart_previews) == Key::r);
	CHECK(bindings.key(GalleryAction::reset_difficulty) == Key::d);
}

TEST_CASE("invalid gallery action identities fail loudly without changing the table")
{
	GalleryBindings bindings;
	const std::array<std::string_view, 2> before = bindings.tokens();
	for (GalleryAction action : { static_cast<GalleryAction>(-1), GalleryAction::count,
		static_cast<GalleryAction>(255) })
	{
		CHECK_THROWS_AS(bindings.key(action), std::invalid_argument);
		CHECK_THROWS_AS(bindings.try_bind(action, Key::a), std::invalid_argument);
		CHECK_THROWS_AS(bindings.try_bind(action, Key::none), std::invalid_argument);
		CHECK(bindings.tokens() == before);
	}
}
