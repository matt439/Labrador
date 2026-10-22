#include <doctest/doctest.h>
#include "engine/assets/json.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
using namespace labrador;

namespace
{
	// A JSON file on disk, deleted when the test leaves. read_json_file takes a
	// path, so testing it means writing files. parse_json is the in-memory door
	// and makes read_json_file's own parse, so the cases that pin what a file
	// does - a missing one, a null path - stay on files, and the parse_json
	// cases check that both doors make the same document from the same bytes.
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

	// What a document made by `make` says its root's "a" is - a whole number,
	// a string, or that the parse failed - so two doors can be compared on
	// bytes whose outcome the test does not want to pin.
	std::string outcome_of(const std::function<JsonDocument()>& make)
	{
		try
		{
			const JsonDocument document = make();
			const JsonValue root = document.root();
			try
			{
				return "integer " + std::to_string(root.integer("a"));
			}
			catch (const std::runtime_error&)
			{
				return "string " + root.string("a");
			}
		}
		catch (const std::runtime_error&)
		{
			return "no document";
		}
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

	TEST_SUITE("ParseJsonTests")
	{
		TEST_CASE("parse_json reads what read_json_file reads from the same bytes")
		{
			const std::string text = R"({
				"name": "turbulence",
				"count": 7,
				"time": 0.05,
				"looping": true,
				"names": [ "a", "b" ]
			})";
			const TempJson file(text);

			const JsonDocument from_file = read_json_file(file.path().c_str());
			const JsonDocument from_text = parse_json(text, "levels/forest.json");
			const JsonValue a = from_file.root();
			const JsonValue b = from_text.root();

			CHECK(a.string("name") == b.string("name"));
			CHECK(a.integer("count") == b.integer("count"));
			CHECK(a.number("time") == b.number("time"));
			CHECK(a.boolean("looping") == b.boolean("looping"));
			REQUIRE(b.array("names").size() == a.array("names").size());
			CHECK(b.array("names").at(1).as_string() == "b");
			CHECK(from_text.source_path() == "levels/forest.json");
		}

		TEST_CASE("the document outlives the caller's text and name")
		{
			std::string text = R"({ "name": "kept" })";
			std::string name = "levels/forest.json";

			const JsonDocument document = parse_json(text, name);
			text.assign(text.size(), 'x');
			name.assign(name.size(), 'x');
			text.clear();
			text.shrink_to_fit();
			name.clear();
			name.shrink_to_fit();

			CHECK(document.root().string("name") == "kept");
			CHECK(document.source_path() == "levels/forest.json");
			CHECK(document.root().where() ==
				"'levels/forest.json': the document");
		}

		TEST_CASE("a parse_json failure names the source, the position and the key")
		{
			const JsonDocument document = parse_json(
				R"({ "assets": [ { "kind": "font" } ] })", "manifest.json");
			const JsonValue asset = document.root().array("assets").at(0);

			CHECK(asset.where() == "'manifest.json': assets[0]");
			CHECK_THROWS_WITH_AS(asset.string("directory"),
				"'manifest.json': assets[0] has no 'directory'",
				std::runtime_error);
			CHECK_THROWS_WITH_AS(asset.integer("kind"),
				"'manifest.json': assets[0].kind is not a whole number",
				std::runtime_error);
		}

		TEST_CASE("text that does not parse names the source and the offset")
		{
			try
			{
				parse_json(R"({ "a": 1, )", "broken.json");
				FAIL("expected a throw");
			}
			catch (const std::runtime_error& error)
			{
				const std::string message = error.what();
				CHECK(message.rfind(
					"parse_json - 'broken.json' is not valid JSON: ", 0) == 0);
				CHECK(message.find("(offset ") != std::string::npos);
			}

			// Empty text is not an empty document, through this door either.
			CHECK_THROWS_AS(parse_json("", "empty.json"), std::runtime_error);
		}

		TEST_CASE("an empty source name is a programming error")
		{
			CHECK_THROWS_AS(parse_json("{}", ""), std::invalid_argument);
			CHECK_THROWS_AS(parse_json("not json", std::string()),
				std::invalid_argument);
		}

		TEST_CASE("read_json_file keeps its own name in its messages")
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
				CHECK(message.rfind("read_json_file - '" + file.path() +
					"' is not valid JSON: ", 0) == 0);
			}
		}

		TEST_CASE("moved parse_json documents keep their source for their values")
		{
			JsonDocument document = parse_json(R"({ "a": { "b": 1 } })",
				"moved.json");
			const JsonValue inner = document.root().object("a");
			const JsonDocument moved = std::move(document);

			CHECK(moved.source_path() == "moved.json");
			CHECK(inner.where() == "'moved.json': a");
			CHECK_THROWS_WITH_AS(inner.string("b"),
				"'moved.json': a.b is not a string", std::runtime_error);

			JsonDocument source = parse_json(R"({ "c": [ 2 ] })",
				"assigned.json");
			const JsonValue c = source.root().array("c");
			JsonDocument target = parse_json("[]", "target.json");
			target = std::move(source);

			CHECK(target.source_path() == "assigned.json");
			CHECK(c.where() == "'assigned.json': c");
			CHECK_THROWS_WITH_AS(c.at(0).as_string(),
				"'assigned.json': c[0] is not a string", std::runtime_error);
		}

		TEST_CASE("both doors agree on a BOM and on NULs")
		{
			// Not a pin on what rapidjson makes of these bytes - only that
			// read_json_file and parse_json make the same of them, because
			// they share one parse.
			const std::string cases[] = {
				std::string("\xEF\xBB\xBF{ \"a\": 1 }"),
				std::string("{ \"a\": 1 }\0junk", 15),
				std::string("{ \"a\": \"x\0y\" }", 14),
			};

			for (const std::string& text : cases)
			{
				CAPTURE(text);
				const TempJson file(text);
				const std::string from_file = outcome_of(
					[&file] { return read_json_file(file.path().c_str()); });
				const std::string from_text = outcome_of(
					[&text] { return parse_json(text, "edge.json"); });

				CHECK(from_file == from_text);
			}
		}
	}
}


TEST_CASE("JSON strings retain embedded NUL while identifier access rejects it")
{
    const TempJson file(R"({"name":"asset\u0000other","names":["asset\u0000other"]})");
    const JsonDocument document = read_json_file(file.path().c_str());
    CHECK(document.root().string("name") == std::string("asset\0other", 11));
    CHECK(document.root().array("names").at(0).as_string() == std::string("asset\0other", 11));
    CHECK_THROWS_WITH_AS(document.root().identifier("name"),
        doctest::Contains("name contains an embedded NUL"), std::runtime_error);
    CHECK_THROWS_WITH_AS(document.root().array("names").at(0).as_identifier(),
        doctest::Contains("names[0] contains an embedded NUL"), std::runtime_error);
}
