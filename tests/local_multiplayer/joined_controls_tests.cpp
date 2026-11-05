#include <doctest/doctest.h>

#include "samples/local_multiplayer/controls.h"

#include <array>

using namespace labrador;
using namespace multiplayer;

TEST_CASE("joined movement follows source ownership rather than player or pad index")
{
	Keyboard keyboard;
	keyboard.set_focused(true);
	keyboard.on_key_down(Key::d);
	keyboard.on_key_down(Key::up);
	keyboard.poll();
	const std::array<Vector2F, Gamepads::max_count> sticks = {
		Vector2F(-1.0f, 0.0f), Vector2F(0.0f, 1.0f),
		Vector2F(1.0f, 0.0f), Vector2F(0.0f, -1.0f)
	};
	const std::array<InputSource, Arena::player_count> sources = {
		InputSource::pad_3, InputSource::keyboard_left
	};
	const std::array<Vector2F, Arena::player_count> directions =
		read_bound_directions(keyboard, sticks, sources);
	CHECK(directions[0] == Vector2F(0.0f, -1.0f));
	CHECK(directions[1] == Vector2F(1.0f, 0.0f));
}

TEST_CASE("either keyboard half can own either joined player")
{
	Keyboard keyboard;
	keyboard.set_focused(true);
	keyboard.on_key_down(Key::a);
	keyboard.on_key_down(Key::down);
	keyboard.poll();
	const std::array<Vector2F, Gamepads::max_count> sticks = {};
	const std::array<InputSource, Arena::player_count> sources = {
		InputSource::keyboard_right, InputSource::keyboard_left
	};
	const std::array<Vector2F, Arena::player_count> directions =
		read_bound_directions(keyboard, sticks, sources);
	CHECK(directions[0] == Vector2F(0.0f, 1.0f));
	CHECK(directions[1] == Vector2F(-1.0f, 0.0f));
}

TEST_CASE("unjoined sources contribute nothing and assigned pads keep the movement deadzone")
{
	Keyboard keyboard;
	keyboard.set_focused(true);
	keyboard.on_key_down(Key::d);
	keyboard.on_key_down(Key::up);
	keyboard.poll();
	const std::array<Vector2F, Gamepads::max_count> sticks = {
		Vector2F(1.0f, 0.0f), Vector2F(0.1f, 0.0f),
		Vector2F::zero, Vector2F::zero
	};
	const std::array<InputSource, Arena::player_count> sources = {
		InputSource::none, InputSource::pad_1
	};
	const std::array<Vector2F, Arena::player_count> directions =
		read_bound_directions(keyboard, sticks, sources);
	CHECK(directions[0] == Vector2F::zero);
	CHECK(directions[1] == Vector2F::zero);
}

TEST_CASE("each keyboard half's keys come from one table, and a pad has none")
{
	const KeyBindings* left = key_bindings(InputSource::keyboard_left);
	const KeyBindings* right = key_bindings(InputSource::keyboard_right);
	REQUIRE(left != nullptr);
	REQUIRE(right != nullptr);
	CHECK(left == &keyboard_bindings[source_index(InputSource::keyboard_left)]);
	CHECK(right == &keyboard_bindings[source_index(InputSource::keyboard_right)]);
	CHECK(left->up == Key::w);
	CHECK(left->confirm == Key::f);
	CHECK(left->leave == Key::g);
	CHECK(right->down == Key::down);
	CHECK(right->confirm == Key::enter);
	CHECK(right->leave == Key::backspace);
	CHECK(key_bindings(InputSource::pad_0) == nullptr);
	CHECK(key_bindings(InputSource::pad_3) == nullptr);
	CHECK(key_bindings(InputSource::none) == nullptr);

	// The source order is the index of every per-source array.
	CHECK(source_index(InputSource::keyboard_left) == 0);
	CHECK(source_index(InputSource::pad_0) + 3 == source_index(InputSource::pad_3));
}
