#include <doctest/doctest.h>
#include "engine/assets/json.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
using namespace labrador;

namespace
{
	// A JSON file on disk, deleted when the test leaves. read_json_file takes a
	// path, so testing it means writing files - there is no in-memory door in,
	// and adding one for the tests would be a second parse path to keep honest.
	class TempJson
	{
	public:
		explicit TempJson(const std::string& contents)
		{
			this->path_ = std::filesystem::temp_directory_path() /
				("labrador_json_test_" + std::to_string(next_id()) + ".json");

			std::ofstream file(this->path_, std::ios::binary);
			file << contents;
		}

		~TempJson()
		{
			std::error_code ignored;
			std::filesystem::remove(this->path_, ignored);
		}

		TempJson(const TempJson&) = delete;
		TempJson& operator=(const TempJson&) = delete;

		std::string path() const { return this->path_.string(); }

	private:
		static int next_id()
		{
			static int id = 0;
			return ++id;
		}

		std::filesystem::path path_;
	};

	// What a caller sees when a read fails. Every case below asserts on the
	// message rather than only on the type, because the message is the whole
	// point: an assert would have said nothing at all, and would have said it
	// by dereferencing null in a Release build.
	std::string message_from(const std::string& contents,
		const std::function<void(const JsonValue&)>& read)
	{
		const TempJson file(contents);
		try
		{
			const JsonDocument document = read_json_file(file.path().c_str());
			read(document.root());
		}
		catch (const std::runtime_error& error)
		{
			std::string message = error.what();

			// The file is named in every message, and it is the part a test
			// cannot spell out, so check it here and take it back out.
			const size_t path = message.find(file.path());
			CHECK(path != std::string::npos);
			if (path != std::string::npos)
			{
				message.replace(path, file.path().size(), "<file>");
			}
			return message;
		}
		FAIL("expected a throw");
		return {};
	}
}

namespace JsonTests
{
	TEST_SUITE("JsonTests")
	{
		TEST_CASE("a file that is not there names itself")
		{
			CHECK_THROWS_AS(read_json_file("./no_such_file.json"),
				std::runtime_error);
		}

		TEST_CASE("a null path is a programming error, not a content one")
		{
			CHECK_THROWS_AS(read_json_file(nullptr), std::invalid_argument);
		}

		TEST_CASE("an empty file is not an empty document")
		{
			// A zero-byte content file is what a truncated write or a failed
			// copy leaves behind, and it is not the same thing as `{}`.
			const TempJson file("");

			CHECK_THROWS_AS(read_json_file(file.path().c_str()),
				std::runtime_error);
		}

		TEST_CASE("a parse error names the file and where in it")
		{
			const TempJson file(R"({ "a": 1, )");
			try
			{
				read_json_file(file.path().c_str());
				FAIL("expected a throw");
			}
			catch (const std::runtime_error& error)
			{
				const std::string message = error.what();
				CHECK(message.find(file.path()) != std::string::npos);
				CHECK(message.find("offset") != std::string::npos);
			}
		}

		TEST_CASE("every scalar comes back as itself")
		{
			const TempJson file(R"({
				"name": "turbulence",
				"count": 7,
				"time": 0.05,
				"looping": true
			})");

			const JsonDocument document = read_json_file(file.path().c_str());
			const JsonValue root = document.root();

			CHECK(root.string("name") == "turbulence");
			CHECK(root.integer("count") == 7);
			CHECK(root.number("time") == doctest::Approx(0.05f));
			CHECK(root.boolean("looping"));
		}

		TEST_CASE("a whole number is still a number")
		{
			// `0` and `0.0` mean the same thing to a person writing a level
			// file, and rapidjson's IsFloat() is false for the first of them.
			const TempJson file(R"({ "x": 0, "y": -3 })");

			const JsonDocument document = read_json_file(file.path().c_str());

			CHECK(document.root().number("x") == doctest::Approx(0.0f));
			CHECK(document.root().number("y") == doctest::Approx(-3.0f));
		}

		TEST_CASE("a number with a fraction is not a whole number")
		{
			CHECK(message_from(R"({ "count": 1.5 })",
				[](const JsonValue& root) { root.integer("count"); }) ==
				"'<file>': count is not a whole number");
		}

		TEST_CASE("arrays are walked by index")
		{
			const TempJson file(R"({ "names": [ "a", "b", "c" ] })");

			const JsonDocument document = read_json_file(file.path().c_str());
			const JsonValue names = document.root().array("names");

			REQUIRE(names.size() == 3);
			CHECK(names.at(0).as_string() == "a");
			CHECK(names.at(2).as_string() == "c");
		}

		TEST_CASE("has() is the only door to a field a file may leave out")
		{
			const TempJson file(R"({ "rotated": false })");

			const JsonDocument document = read_json_file(file.path().c_str());
			const JsonValue root = document.root();

			CHECK(root.has("rotated"));
			CHECK_FALSE(root.has("origin"));

			// Present and false is not the same as absent, and a reader that
			// conflated them would rotate nothing that asked not to be.
			CHECK_FALSE(root.boolean("rotated"));
		}

		TEST_CASE("a missing member names the file, the position and the key")
		{
			CHECK(message_from(R"({ "assets": [ { "kind": "font" } ] })",
				[](const JsonValue& root)
				{
					root.array("assets").at(0).string("directory");
				}) == "'<file>': assets[0] has no 'directory'");
		}

		TEST_CASE("the position is written the way a person would point at it")
		{
			CHECK(message_from(R"({ "a": { "b": [ 1, { "c": [ 2 ] } ] } })",
				[](const JsonValue& root)
				{
					root.object("a").array("b").at(1).array("c").at(0)
						.as_string();
				}) == "'<file>': a.b[1].c[0] is not a string");
		}

		TEST_CASE("the root prints as the document, not as an empty name")
		{
			CHECK(message_from(R"([ 1, 2 ])",
				[](const JsonValue& root) { root.string("name"); }) ==
				"'<file>': the document is not an object");

			CHECK(message_from(R"({ "a": 1 })",
				[](const JsonValue& root) { root.string("name"); }) ==
				"'<file>': the document has no 'name'");
		}

		TEST_CASE("a member of the wrong type says which type it needed")
		{
			const std::string document = R"({
				"object": [],
				"array": {},
				"string": 1,
				"integer": "1",
				"number": "1",
				"boolean": 1
			})";

			CHECK(message_from(document,
				[](const JsonValue& root) { root.object("object"); }) ==
				"'<file>': object is not an object");
			CHECK(message_from(document,
				[](const JsonValue& root) { root.array("array"); }) ==
				"'<file>': array is not an array");
			CHECK(message_from(document,
				[](const JsonValue& root) { root.string("string"); }) ==
				"'<file>': string is not a string");
			CHECK(message_from(document,
				[](const JsonValue& root) { root.integer("integer"); }) ==
				"'<file>': integer is not a whole number");
			CHECK(message_from(document,
				[](const JsonValue& root) { root.number("number"); }) ==
				"'<file>': number is not a number");
			CHECK(message_from(document,
				[](const JsonValue& root) { root.boolean("boolean"); }) ==
				"'<file>': boolean is not true or false");
		}

		TEST_CASE("reading a scalar as a container says so rather than asserting")
		{
			// These are the calls rapidjson answers with RAPIDJSON_ASSERT, so
			// they are the ones a Release build would have walked straight past.
			const std::string document = R"({ "x": 3 })";

			CHECK(message_from(document,
				[](const JsonValue& root) { root.object("x").string("y"); }) ==
				"'<file>': x is not an object");
			CHECK(message_from(document,
				[](const JsonValue& root) { root.array("x").size(); }) ==
				"'<file>': x is not an array");
			CHECK(message_from(document,
				[](const JsonValue& root)
				{
					root.object("x").has("y");
				}) == "'<file>': x is not an object");
		}

		TEST_CASE("an index past the end of an array says which index")
		{
			CHECK(message_from(R"({ "names": [ "a" ] })",
				[](const JsonValue& root) { root.array("names").at(1); }) ==
				"'<file>': names has no element 1");
		}

		TEST_CASE("where() is the same position a failure would have named")
		{
			// The game's own object builders reject values this module has no
			// opinion about - an unknown colour name, an unknown object type -
			// and this is what lets their messages point at the same place.
			const TempJson file(R"({ "objects": [ { "type": "Nope" } ] })");

			const JsonDocument document = read_json_file(file.path().c_str());
			const JsonValue object = document.root().array("objects").at(0);

			CHECK(object.where() == "'" + file.path() + "': objects[0]");
		}

		TEST_CASE("a document remembers the path it was read from")
		{
			const TempJson file(R"({})");

			const JsonDocument document = read_json_file(file.path().c_str());

			CHECK(document.source_path() == file.path());
		}

		TEST_CASE("a moved document keeps its values alive")
		{
			// JsonValue points into the document, and the loaders all take one
			// by value out of read_json_file, so the move had better not be the
			// thing that dangles them.
			const TempJson file(R"({ "name": "kept" })");

			JsonDocument document = read_json_file(file.path().c_str());
			const JsonValue root = document.root();
			const JsonDocument moved = std::move(document);

			CHECK(root.string("name") == "kept");
			CHECK(moved.root().string("name") == "kept");
		}
	}
}
