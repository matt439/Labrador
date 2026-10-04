#include "tools/linesweeper_capture/scripted_match.h"

#include "samples/linesweeper/rules/tick.h"

#include <array>
#include <stdexcept>
#include <string>
#include <utility>

using linesweeper::Coord;
using linesweeper::Kind;
using linesweeper::Piece;

namespace capture
{
	namespace
	{
		[[noreturn]] void refuse(const Command& command,
			const std::string& message)
		{
			throw std::runtime_error("script line " +
				std::to_string(command.line) + ": " + message);
		}

		std::string named(Kind kind)
		{
			return std::string(1, kind_letter(kind));
		}

		int leftmost_column(const Piece& piece)
		{
			const std::array<Coord, linesweeper::piece_cell_count> cells =
				linesweeper::piece_cells(piece);

			int column = cells[0].x;
			for (int index = 1; index < linesweeper::piece_cell_count; ++index)
			{
				column = cells[index].x < column ? cells[index].x : column;
			}

			return column;
		}
	}

	ScriptedMatch::ScriptedMatch(std::function<void()> after_tick) :
		after_tick_(std::move(after_tick))
	{

	}

	const linesweeper::World& ScriptedMatch::world() const
	{
		return this->world_;
	}

	const linesweeper::TickResult& ScriptedMatch::last_tick() const
	{
		return this->last_tick_;
	}

	void ScriptedMatch::run(const Command& command)
	{
		switch (command.kind)
		{
		case CommandKind::place:
			this->steer(command);
			this->drop(command);
			break;

		case CommandKind::steer:
			this->steer(command);
			break;

		case CommandKind::drop:
			this->drop(command);
			break;

		case CommandKind::hold:
			this->hold(command);
			break;

		case CommandKind::wait:
			for (int index = 0; index < command.ticks; ++index)
			{
				this->step(linesweeper::button_none);
			}
			break;

		case CommandKind::capture:
		default:
			throw std::logic_error("ScriptedMatch::run was handed a capture, "
				"which is the caller's.");
		}
	}

	void ScriptedMatch::step(std::uint8_t input)
	{
		this->last_tick_ = linesweeper::tick(this->world_, input);
		if (this->last_tick_.locked.kind != Kind::none)
		{
			++this->locks_;
		}
		this->after_tick_();
	}

	void ScriptedMatch::tap(std::uint8_t button)
	{
		this->step(button);
		this->step(linesweeper::button_none);
	}

	void ScriptedMatch::take_piece(const Command& command, Kind expected)
	{
		if (this->world_.topped_out == 0 &&
			this->world_.current.kind == Kind::none)
		{
			this->step(linesweeper::button_none);
		}

		if (this->world_.topped_out != 0)
		{
			refuse(command, "the match is already over");
		}

		if (expected != Kind::none && this->world_.current.kind != expected)
		{
			refuse(command, "the script expects " + named(expected) +
				" but the falling piece is " +
				named(this->world_.current.kind));
		}
	}

	void ScriptedMatch::steer(const Command& command)
	{
		this->take_piece(command, command.piece);

		const int locks = this->locks_;

		// Three clockwise turns are one anticlockwise one, which is the press a
		// player would make and leaves two fewer ticks for gravity.
		if (command.turns == 3)
		{
			this->tap(linesweeper::button_rotate_anticlockwise);
		}
		else
		{
			for (int turn = 0; turn < command.turns; ++turn)
			{
				this->tap(linesweeper::button_rotate_clockwise);
			}
		}

		if (this->locks_ != locks)
		{
			refuse(command, "the piece locked before it had turned");
		}

		if (this->world_.current.rotation != command.turns)
		{
			refuse(command, "the piece did not turn " +
				std::to_string(command.turns) +
				" times; the well blocked every kick");
		}

		while (leftmost_column(this->world_.current) != command.column)
		{
			const int before = leftmost_column(this->world_.current);

			this->tap(before > command.column
				? linesweeper::button_left
				: linesweeper::button_right);

			if (this->locks_ != locks)
			{
				refuse(command, "the piece locked at column " +
					std::to_string(before) + " before it reached column " +
					std::to_string(command.column));
			}

			if (leftmost_column(this->world_.current) == before)
			{
				refuse(command, "the well blocks the piece at column " +
					std::to_string(before) + " on its way to column " +
					std::to_string(command.column));
			}
		}
	}

	void ScriptedMatch::drop(const Command& command)
	{
		this->take_piece(command, Kind::none);

		// The press locks the piece and the release deals the next one, so a
		// drop is two ticks like every other tap - and the second is the first
		// tick of whatever the drop threw, which a capture right after it sees.
		this->tap(linesweeper::button_hard_drop);
	}

	void ScriptedMatch::hold(const Command& command)
	{
		this->take_piece(command, command.piece);

		if (this->world_.hold_available == 0)
		{
			refuse(command,
				"this piece came out of hold, and a piece may be held once");
		}

		this->tap(linesweeper::button_hold);

		if (this->world_.hold_kind != static_cast<std::uint8_t>(command.piece))
		{
			refuse(command, "the hold did not take " + named(command.piece));
		}
	}
}
