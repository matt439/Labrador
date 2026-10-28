#include <doctest/doctest.h>

#include "samples/local_multiplayer/join_rules.h"

#include <array>
#include <cstddef>
#include <initializer_list>
#include <stdexcept>

using namespace multiplayer;

namespace
{
	using JoinSnapshot = std::array<JoinInput, input_source_count>;

	JoinInput& source(JoinSnapshot& input, InputSource selected)
	{
		return input.at(static_cast<std::size_t>(selected));
	}

	JoinSnapshot two_pads()
	{
		JoinSnapshot input = {};
		for (const InputSource selected : { InputSource::pad_0, InputSource::pad_3 })
		{
			source(input, selected).connected = true;
			source(input, selected).join_pressed = true;
		}
		return input;
	}
}

TEST_CASE("couch sources join in deterministic order without exceeding two players")
{
	JoinSnapshot input = {};
	for (JoinInput& entry : input)
	{
		entry.connected = true;
		entry.join_pressed = true;
	}
	const Lobby lobby = update_lobby(Lobby{}, input);
	CHECK(lobby.players[0].source == InputSource::keyboard_left);
	CHECK(lobby.players[1].source == InputSource::keyboard_right);
	CHECK_FALSE(lobby.players[0].ready);
	CHECK_FALSE(lobby.players[1].ready);
	CHECK_FALSE(all_ready(lobby));
}

TEST_CASE("a fourth pad slot can be the first player's source")
{
	JoinSnapshot input = {};
	source(input, InputSource::pad_3).connected = true;
	source(input, InputSource::pad_3).join_pressed = true;
	const Lobby lobby = update_lobby(Lobby{}, input);
	CHECK(lobby.players[0].source == InputSource::pad_3);
	CHECK(lobby.players[1].source == InputSource::none);
	CHECK(pad_slot(lobby.players[0].source) == 3);
}

TEST_CASE("one source cannot claim another player or reset its ready choice by joining twice")
{
	JoinSnapshot input = {};
	source(input, InputSource::keyboard_right).connected = true;
	source(input, InputSource::keyboard_right).join_pressed = true;
	Lobby lobby = update_lobby(Lobby{}, input);
	lobby = choose_lobby(lobby, 0, JoinChoice::ready);
	lobby = update_lobby(lobby, input);
	CHECK(lobby.players[0].source == InputSource::keyboard_right);
	CHECK(lobby.players[0].ready);
	CHECK(lobby.players[1].source == InputSource::none);
	CHECK_FALSE(all_ready(lobby));
}

TEST_CASE("joining requires a connected source and a fresh press outside connection edges")
{
	JoinSnapshot input = {};
	source(input, InputSource::pad_0).join_pressed = true;
	CHECK(update_lobby(Lobby{}, input).players[0].source == InputSource::none);
	source(input, InputSource::pad_0).connected = true;
	source(input, InputSource::pad_0).just_connected = true;
	CHECK(update_lobby(Lobby{}, input).players[0].source == InputSource::none);
	source(input, InputSource::pad_0).just_connected = false;
	source(input, InputSource::pad_0).just_disconnected = true;
	CHECK(update_lobby(Lobby{}, input).players[0].source == InputSource::none);
	source(input, InputSource::pad_0).just_disconnected = false;
	source(input, InputSource::pad_0).join_pressed = false;
	CHECK(update_lobby(Lobby{}, input).players[0].source == InputSource::none);
	source(input, InputSource::pad_0).join_pressed = true;
	CHECK(update_lobby(Lobby{}, input).players[0].source == InputSource::pad_0);
}

TEST_CASE("per-player ready choices remain independent and both players must confirm")
{
	const JoinSnapshot input = two_pads();
	const Lobby joined = update_lobby(Lobby{}, input);
	const Lobby second_ready = choose_lobby(joined, 1, JoinChoice::ready);
	CHECK_FALSE(joined.players[1].ready);
	CHECK_FALSE(second_ready.players[0].ready);
	CHECK(second_ready.players[1].ready);
	CHECK_FALSE(all_ready(second_ready));
	const Lobby both_ready = choose_lobby(second_ready, 0, JoinChoice::ready);
	CHECK(all_ready(both_ready));
	CHECK(both_ready.players[0].source == InputSource::pad_0);
	CHECK(both_ready.players[1].source == InputSource::pad_3);
}

TEST_CASE("leaving clears only that player's source and readiness and reuses the vacant slot")
{
	JoinSnapshot input = two_pads();
	Lobby lobby = update_lobby(Lobby{}, input);
	lobby = choose_lobby(lobby, 0, JoinChoice::ready);
	lobby = choose_lobby(lobby, 1, JoinChoice::ready);
	lobby = choose_lobby(lobby, 0, JoinChoice::leave);
	CHECK(lobby.players[0].source == InputSource::none);
	CHECK_FALSE(lobby.players[0].ready);
	CHECK(lobby.players[1].source == InputSource::pad_3);
	CHECK(lobby.players[1].ready);
	source(input, InputSource::pad_0).join_pressed = false;
	source(input, InputSource::keyboard_left).connected = true;
	source(input, InputSource::keyboard_left).join_pressed = true;
	lobby = update_lobby(lobby, input);
	CHECK(lobby.players[0].source == InputSource::keyboard_left);
	CHECK_FALSE(lobby.players[0].ready);
	CHECK(lobby.players[1].ready);
}

TEST_CASE("disconnect mid-menu drops only that source and cancels its ready choice")
{
	JoinSnapshot input = two_pads();
	Lobby lobby = update_lobby(Lobby{}, input);
	lobby = choose_lobby(lobby, 0, JoinChoice::ready);
	lobby = choose_lobby(lobby, 1, JoinChoice::ready);
	CHECK(all_ready(lobby));
	source(input, InputSource::pad_0).connected = false;
	source(input, InputSource::pad_0).just_disconnected = true;
	lobby = update_lobby(lobby, input);
	CHECK(lobby.players[0].source == InputSource::none);
	CHECK_FALSE(lobby.players[0].ready);
	CHECK(lobby.players[1].source == InputSource::pad_3);
	CHECK(lobby.players[1].ready);
	CHECK_FALSE(all_ready(lobby));
}

TEST_CASE("an observed disconnect removes an assignment even if the current connected value is true")
{
	JoinSnapshot input = two_pads();
	Lobby lobby = update_lobby(Lobby{}, input);
	source(input, InputSource::pad_3).just_disconnected = true;
	lobby = update_lobby(lobby, input);
	CHECK(lobby.players[0].source == InputSource::pad_0);
	CHECK(lobby.players[1].source == InputSource::none);
}

TEST_CASE("a disconnected source does not reclaim its old player on replug without a new press")
{
	JoinSnapshot input = two_pads();
	Lobby lobby = update_lobby(Lobby{}, input);
	source(input, InputSource::pad_0).connected = false;
	lobby = update_lobby(lobby, input);
	source(input, InputSource::pad_0).connected = true;
	source(input, InputSource::pad_0).just_connected = true;
	lobby = update_lobby(lobby, input);
	CHECK(lobby.players[0].source == InputSource::none);
	source(input, InputSource::pad_0).just_connected = false;
	source(input, InputSource::pad_0).join_pressed = false;
	lobby = update_lobby(lobby, input);
	CHECK(lobby.players[0].source == InputSource::none);
	source(input, InputSource::pad_0).join_pressed = true;
	lobby = update_lobby(lobby, input);
	CHECK(lobby.players[0].source == InputSource::pad_0);
	CHECK_FALSE(lobby.players[0].ready);
}

TEST_CASE("disconnect is applied before a different fresh source claims the vacant player")
{
	JoinSnapshot input = two_pads();
	Lobby lobby = update_lobby(Lobby{}, input);
	lobby = choose_lobby(lobby, 1, JoinChoice::ready);
	source(input, InputSource::pad_0).connected = false;
	source(input, InputSource::pad_2).connected = true;
	source(input, InputSource::pad_2).join_pressed = true;
	lobby = update_lobby(lobby, input);
	CHECK(lobby.players[0].source == InputSource::pad_2);
	CHECK_FALSE(lobby.players[0].ready);
	CHECK(lobby.players[1].source == InputSource::pad_3);
	CHECK(lobby.players[1].ready);
}

TEST_CASE("empty player choices cannot bypass joining and player indices are checked")
{
	const Lobby empty;
	CHECK_FALSE(all_ready(choose_lobby(empty, 0, JoinChoice::ready)));
	CHECK(choose_lobby(empty, 1, JoinChoice::leave).players[1].source == InputSource::none);
	CHECK_THROWS_AS(choose_lobby(empty, Arena::player_count, JoinChoice::ready), std::out_of_range);
}

TEST_CASE("pad source mapping keeps physical slots separate from player slots")
{
	CHECK(pad_slot(InputSource::keyboard_left) == -1);
	CHECK(pad_slot(InputSource::keyboard_right) == -1);
	CHECK(pad_slot(InputSource::none) == -1);
	CHECK(pad_slot(InputSource::pad_0) == 0);
	CHECK(pad_slot(InputSource::pad_1) == 1);
	CHECK(pad_slot(InputSource::pad_2) == 2);
	CHECK(pad_slot(InputSource::pad_3) == 3);
	CHECK(pad_slot(static_cast<InputSource>(-1)) == -1);
	CHECK(pad_slot(static_cast<InputSource>(99)) == -1);
}
