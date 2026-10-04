#include "tools/linesweeper_capture/capture_script.h"

#include <cstddef>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using linesweeper::Kind;

namespace capture
{
	namespace
	{
		// In Kind's own order, so a letter's index is its enumerator less one.
		const char* const kind_letters = "ijlostz";

		[[noreturn]] void refuse(const std::filesystem::path& path, int line,
			const std::string& message)
		{
			throw std::runtime_error(path.string() + ":" +
				std::to_string(line) + ": " + message);
		}

		Kind parse_kind(const std::filesystem::path& path, int line,
			const std::string& word)
		{
			if (word.size() == 1)
			{
				for (int index = 0; index < linesweeper::kind_count; ++index)
				{
					if (kind_letters[index] == word[0])
					{
						return static_cast<Kind>(index + 1);
					}
				}
			}

			refuse(path, line, "'" + word +
				"' is not a piece; the pieces are i, j, l, o, s, t and z");
		}

		int parse_number(const std::filesystem::path& path, int line,
			const std::string& word, int min, int max, const char* what)
		{
			std::size_t used = 0;
			int value = 0;

			try
			{
				value = std::stoi(word, &used);
			}
			catch (const std::exception&)
			{
				used = 0;
			}

			if (used != word.size() || word.empty() || value < min ||
				value > max)
			{
				refuse(path, line, "'" + word + "' is not a " + what +
					" from " + std::to_string(min) + " to " +
					std::to_string(max));
			}

			return value;
		}

		// A bare name, because the output directory is the caller's to choose
		// and a script must not be able to write outside it.
		void check_file_name(const std::filesystem::path& path, int line,
			const std::string& name)
		{
			const bool bare =
				name.find_first_of("/\\:") == std::string::npos &&
				name != "." && name != "..";
			const bool png = name.size() > 4 &&
				name.compare(name.size() - 4, 4, ".png") == 0;

			if (!bare || !png)
			{
				refuse(path, line, "'" + name +
					"' is not a bare file name ending in .png");
			}
		}
	}

	char kind_letter(Kind kind)
	{
		const int index = static_cast<int>(kind) - 1;

		return index >= 0 && index < linesweeper::kind_count
			? kind_letters[index]
			: '?';
	}

	std::vector<Command> read_script(const std::filesystem::path& path)
	{
		std::ifstream file(path);

		if (!file)
		{
			throw std::runtime_error("Could not open the capture script " +
				path.string() + ".");
		}

		std::vector<Command> commands;
		std::vector<std::string> files;
		std::string text;
		int line = 0;

		while (std::getline(file, text))
		{
			++line;

			const std::size_t comment = text.find('#');
			if (comment != std::string::npos)
			{
				text.erase(comment);
			}

			std::istringstream stream(text);
			std::vector<std::string> words;
			std::string word;
			while (stream >> word)
			{
				words.push_back(word);
			}

			if (words.empty())
			{
				continue;
			}

			Command command;
			command.line = line;

			const std::string& verb = words[0];
			std::size_t expected = 0;

			if (verb == "place" || verb == "steer")
			{
				command.kind = verb == "place"
					? CommandKind::place
					: CommandKind::steer;
				expected = 4;
				if (words.size() == expected)
				{
					command.piece = parse_kind(path, line, words[1]);
					command.turns = parse_number(path, line, words[2], 0, 3,
						"number of clockwise turns");
					command.column = parse_number(path, line, words[3], 0,
						linesweeper::well_columns - 1, "column");
				}
			}
			else if (verb == "drop")
			{
				command.kind = CommandKind::drop;
				expected = 1;
			}
			else if (verb == "hold")
			{
				command.kind = CommandKind::hold;
				expected = 2;
				if (words.size() == expected)
				{
					command.piece = parse_kind(path, line, words[1]);
				}
			}
			else if (verb == "wait")
			{
				command.kind = CommandKind::wait;
				expected = 2;
				if (words.size() == expected)
				{
					command.ticks = parse_number(path, line, words[1], 1,
						100000, "number of ticks");
				}
			}
			else if (verb == "capture")
			{
				command.kind = CommandKind::capture;
				expected = words.size() == 3 ? 3 : 2;
				if (words.size() == 3 && words[2] != "paused")
				{
					refuse(path, line, "'" + words[2] +
						"' is not a capture option; the only one is paused");
				}
				if (words.size() >= 2)
				{
					check_file_name(path, line, words[1]);
					command.file = words[1];
					command.paused = words.size() == 3;

					for (const std::string& earlier : files)
					{
						if (earlier == command.file)
						{
							refuse(path, line, command.file +
								" is captured twice, and the second would "
								"overwrite the first");
						}
					}
					files.push_back(command.file);
				}
			}
			else
			{
				refuse(path, line, "'" + verb + "' is not a command; the "
					"commands are place, steer, drop, hold, wait and capture");
			}

			if (words.size() != expected)
			{
				refuse(path, line, "'" + verb + "' takes " +
					std::to_string(expected - 1) + " argument" +
					(expected == 2 ? "" : "s") + ", not " +
					std::to_string(words.size() - 1));
			}

			commands.push_back(command);
		}

		if (files.empty())
		{
			throw std::runtime_error(path.string() +
				" captures nothing, so running it would write nothing.");
		}

		return commands;
	}
}
