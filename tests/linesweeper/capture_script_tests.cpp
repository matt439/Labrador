#include <doctest/doctest.h>
#include "tools/linesweeper_capture/capture_script.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

// The capture tool's script reader. Its refusals are what a person editing a
// script reads, so each one has to say what is wrong with that line.
namespace
{
	// A script in a file of its own, removed when the case ends.
	class ScriptFile final
	{
	public:
		explicit ScriptFile(const std::string& text) :
			path_(std::filesystem::temp_directory_path() /
				("labrador-capture-script-" + std::to_string(
					std::chrono::steady_clock::now().time_since_epoch().count()) +
					".txt"))
		{
			std::ofstream file(this->path_);
			file << text;
		}

		~ScriptFile()
		{
			std::error_code ignored;
			std::filesystem::remove(this->path_, ignored);
		}

		ScriptFile(const ScriptFile&) = delete;
		ScriptFile& operator=(const ScriptFile&) = delete;

		const std::filesystem::path& path() const
		{
			return this->path_;
		}

	private:
		std::filesystem::path path_;
	};
}

TEST_CASE("a capture takes a file name and optionally paused, and says so")
{
	SUBCASE("one or two arguments are read")
	{
		const ScriptFile script("capture first.png\ncapture second.png paused\n");
		const std::vector<capture::Command> commands =
			capture::read_script(script.path());

		REQUIRE(commands.size() == 2);
		CHECK(commands[0].kind == capture::CommandKind::capture);
		CHECK(commands[0].file == "first.png");
		CHECK_FALSE(commands[0].paused);
		CHECK(commands[1].file == "second.png");
		CHECK(commands[1].paused);
	}

	SUBCASE("three arguments are refused with what capture takes")
	{
		const ScriptFile script("capture first.png paused extra\n");
		CHECK_THROWS_WITH_AS(capture::read_script(script.path()),
			doctest::Contains(":1: 'capture' takes a file name and, optionally, "
				"paused; not 3 arguments"),
			std::runtime_error);
	}

	SUBCASE("no argument is refused the same way")
	{
		const ScriptFile script("capture\n");
		CHECK_THROWS_WITH_AS(capture::read_script(script.path()),
			doctest::Contains(":1: 'capture' takes a file name and, optionally, "
				"paused; not 0 arguments"),
			std::runtime_error);
	}
}
