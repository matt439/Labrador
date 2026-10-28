#pragma once

#include "samples/local_multiplayer/arena.h"

#include <array>
#include <cstddef>

namespace multiplayer
{
	// Sources are game policy. Their order decides simultaneous joins; a
	// source keeps its player slot until leaving or an observed disconnect.
	enum class InputSource
	{
		keyboard_left = 0,
		keyboard_right,
		pad_0,
		pad_1,
		pad_2,
		pad_3,
		none,
	};

	inline constexpr std::size_t input_source_count = 6;

	// One polled source. join_pressed is an edge, never a held button. A
	// connection or disconnection edge suppresses joining on that poll.
	struct JoinInput
	{
		bool connected = false;
		bool just_connected = false;
		bool just_disconnected = false;
		bool join_pressed = false;
	};

	struct JoinedPlayer
	{
		InputSource source = InputSource::none;
		bool ready = false;
	};

	struct Lobby
	{
		std::array<JoinedPlayer, Arena::player_count> players = {};
	};

	enum class JoinChoice
	{
		ready,
		leave,
	};

	// Remove absent/disconnected assignments first, then bind fresh joins in
	// source order to the first free player. One source cannot join twice.
	// Joining consumes no menu choice and never makes that player ready.
	Lobby update_lobby(Lobby lobby,
		const std::array<JoinInput, input_source_count>& input);

	// READY affects only an occupied player; LEAVE clears that assignment and
	// its readiness. Throws std::out_of_range for an invalid player index.
	Lobby choose_lobby(Lobby lobby, std::size_t player, JoinChoice choice);

	// Both of this sample's player slots must have a source and be ready.
	bool all_ready(const Lobby& lobby);

	// Physical pad slot for a pad source; -1 for keyboards, none or an invalid
	// source. Player slot numbers are independent of these pad slot numbers.
	int pad_slot(InputSource source);
}
