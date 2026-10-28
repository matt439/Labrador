#include "samples/local_multiplayer/join_rules.h"

#include <stdexcept>

namespace multiplayer
{
	static_assert(input_source_count == static_cast<std::size_t>(InputSource::none));

	Lobby update_lobby(Lobby lobby,
		const std::array<JoinInput, input_source_count>& input)
	{
		for (JoinedPlayer& player : lobby.players)
		{
			if (player.source == InputSource::none)
			{
				continue;
			}
			const JoinInput& source = input.at(static_cast<std::size_t>(player.source));
			if (!source.connected || source.just_disconnected)
			{
				player = JoinedPlayer{};
			}
		}

		for (std::size_t index = 0; index < input.size(); ++index)
		{
			const JoinInput& source_input = input[index];
			if (!source_input.connected || source_input.just_connected ||
				source_input.just_disconnected || !source_input.join_pressed)
			{
				continue;
			}

			const InputSource source = static_cast<InputSource>(index);
			bool assigned = false;
			for (const JoinedPlayer& player : lobby.players)
			{
				assigned = assigned || player.source == source;
			}
			if (assigned)
			{
				continue;
			}

			for (JoinedPlayer& player : lobby.players)
			{
				if (player.source == InputSource::none)
				{
					player = JoinedPlayer{ source, false };
					break;
				}
			}
		}
		return lobby;
	}

	Lobby choose_lobby(Lobby lobby, std::size_t player, JoinChoice choice)
	{
		JoinedPlayer& selected = lobby.players.at(player);
		if (selected.source == InputSource::none)
		{
			return lobby;
		}
		switch (choice)
		{
		case JoinChoice::ready:
			selected.ready = true;
			return lobby;
		case JoinChoice::leave:
			selected = JoinedPlayer{};
			return lobby;
		}
		throw std::invalid_argument("Unknown couch join menu choice");
	}

	bool all_ready(const Lobby& lobby)
	{
		for (const JoinedPlayer& player : lobby.players)
		{
			if (player.source == InputSource::none || !player.ready)
			{
				return false;
			}
		}
		return true;
	}

	int pad_slot(InputSource source)
	{
		switch (source)
		{
		case InputSource::pad_0: return 0;
		case InputSource::pad_1: return 1;
		case InputSource::pad_2: return 2;
		case InputSource::pad_3: return 3;
		case InputSource::keyboard_left:
		case InputSource::keyboard_right:
		case InputSource::none:
			return -1;
		}
		return -1;
	}
}
