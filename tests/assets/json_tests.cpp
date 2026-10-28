#include <doctest/doctest.h>
#include "engine/assets/json.h"
#include "tests/scratch_file.h"
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
using namespace labrador;
using labrador_tests::ScratchFile;

namespace
{
	// What a caller sees when a read fails. Every case below asserts on the
	// message rather than only on the type, because the message is the whole
	// point: an assert would have said nothing at all, and would have said it
	// by dereferencing null in a Release build.
	std::string message_from(const std::string& contents,
		const std::function<void(const JsonValue&)>& read)
	{
		const ScratchFile file("document.json", contents);
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
			const ScratchFile file("document.json", "");

			CHECK_THROWS_AS(read_json_file(file.path().c_str()),
				std::runtime_error);
		}

		TEST_CASE("a parse error names the file and where in it")
		{
			const ScratchFile file("document.json", R"({ "a": 1, )");
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
			const ScratchFile file("document.json", R"({
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
			const ScratchFile file("document.json", R"({ "x": 0, "y": -3 })");

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
			const ScratchFile file("document.json", R"({ "names": [ "a", "b", "c" ] })");

			const JsonDocument document = read_json_file(file.path().c_str());
			const JsonValue names = document.root().array("names");

			REQUIRE(names.size() == 3);
			CHECK(names.at(0).as_string() == "a");
			CHECK(names.at(2).as_string() == "c");
		}

		TEST_CASE("has() distinguishes an absent field from a false one")
		{
			const ScratchFile file("document.json", R"({ "rotated": false })");

			const JsonDocument document = read_json_file(file.path().c_str());
			const JsonValue root = document.root();

			CHECK(root.has("rotated"));
			CHECK_FALSE(root.has("origin"));

			// Present and false is not the same as absent, and a reader that
			// conflated them would rotate nothing that asked not to be.
			CHECK_FALSE(root.boolean("rotated"));
		}

		TEST_CASE("keys() lists nested object names in stored order and permits empty objects")
		{
			const JsonDocument document = parse_json(R"({
				"later": 1,
				"group": { "second": 2, "first": 1 },
				"empty": {}
			})", "keys.json");
			const std::vector<std::string> root_names{ "later", "group", "empty" };
			const std::vector<std::string> group_names{ "second", "first" };

			CHECK(document.root().keys() == root_names);
			CHECK(document.root().object("group").keys() == group_names);
			CHECK(document.root().object("empty").keys().empty());
			CHECK(parse_json("{}", "empty.json").root().keys().empty());
		}

		TEST_CASE("keys() preserves decoded Unicode and the lengths of empty and NUL names")
		{
			const JsonDocument document = parse_json(R"({
				"\u00e9": 1,
				"\ud83d\ude80": 2,
				"": 3,
				"part\u0000tail": 4
			})", "decoded-keys.json");
			const std::vector<std::string> names = document.root().keys();

			REQUIRE(names.size() == 4);
			CHECK(names[0] == "\xC3\xA9");
			CHECK(names[1] == "\xF0\x9F\x9A\x80");
			CHECK(names[2].empty());
			CHECK(names[3] == std::string("part\0tail", 9));
		}

		TEST_CASE("keys() retains duplicate decoded member names without deduplicating")
		{
			const JsonDocument document = parse_json(R"({
				"repeat": 1, "other": 2, "repeat": 3, "\u0072epeat": 4
			})", "duplicate-keys.json");
			const std::vector<std::string> expected{
				"repeat", "other", "repeat", "repeat"
			};

			CHECK(document.root().keys() == expected);
		}

		TEST_CASE("keys() returns independent names that outlive the document")
		{
			std::vector<std::string> root_names;
			std::vector<std::string> nested_names;
			{
				const JsonDocument document = parse_json(R"({
					"long_key_whose_storage_must_not_be_borrowed": 1,
					"nested": { "part\u0000tail": 2 }
				})", "owned-keys.json");
				root_names = document.root().keys();
				nested_names = document.root().object("nested").keys();

				std::vector<std::string> changed = document.root().keys();
				REQUIRE_FALSE(changed.empty());
				changed[0] = "changed";
				CHECK(document.root().keys() == root_names);
			}

			REQUIRE(root_names.size() == 2);
			CHECK(root_names[0] == "long_key_whose_storage_must_not_be_borrowed");
			CHECK(root_names[1] == "nested");
			REQUIRE(nested_names.size() == 1);
			CHECK(nested_names[0] == std::string("part\0tail", 9));
		}

		TEST_CASE("keys() rejects non-object roots with their source context")
		{
			const char* contents[] = { "3", "1.5", "true", "\"text\"", "[]", "null" };
			for (const char* text : contents)
			{
				CAPTURE(text);
				const JsonDocument document = parse_json(text, "root-keys.json");
				CHECK_THROWS_WITH_AS(document.root().keys(),
					"'root-keys.json': the document is not an object",
					std::runtime_error);
			}
		}

		TEST_CASE("keys() rejects nested non-objects with the member path and array index")
		{
			const JsonDocument document = parse_json(R"({
				"container": { "items": [ 3, 1.5, true, "text", [], null ] }
			})", "nested-keys.json");
			const JsonValue items = document.root().object("container").array("items");

			for (size_t index = 0; index < items.size(); ++index)
			{
				CAPTURE(index);
				const std::string expected = "'nested-keys.json': container.items[" +
					std::to_string(index) + "] is not an object";
				CHECK_THROWS_WITH_AS(items.at(index).keys(), expected.c_str(),
					std::runtime_error);
			}
		}

		TEST_CASE("keys() leaves game-owned named dictionaries and metadata unrestricted")
		{
			const JsonDocument document = parse_json(R"({
				"resources": {
					"level:one": { "metadata": { "tool": "packer", "arbitrary": true } },
					"new kind": {}
				},
				"metadata": { "anything": [ null ], "vendor": { "revision": 17 } }
			})", "game-content.json");
			const JsonValue root = document.root();
			const JsonValue resources = root.object("resources");
			const std::vector<std::string> resource_names{ "level:one", "new kind" };
			const std::vector<std::string> metadata_names{ "anything", "vendor" };

			CHECK(resources.keys() == resource_names);
			CHECK(resources.object("new kind").keys().empty());
			CHECK(resources.object("level:one").object("metadata").string("tool") == "packer");
			CHECK(resources.object("level:one").object("metadata").boolean("arbitrary"));
			CHECK(root.object("metadata").keys() == metadata_names);
			CHECK(root.object("metadata").array("anything").size() == 1);
			CHECK(root.object("metadata").object("vendor").integer("revision") == 17);
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
			const ScratchFile file("document.json", R"({ "objects": [ { "type": "Nope" } ] })");

			const JsonDocument document = read_json_file(file.path().c_str());
			const JsonValue object = document.root().array("objects").at(0);

			CHECK(object.where() == "'" + file.path() + "': objects[0]");
		}

		TEST_CASE("a document remembers the path it was read from")
		{
			const ScratchFile file("document.json", R"({})");

			const JsonDocument document = read_json_file(file.path().c_str());

			CHECK(document.source_path() == file.path());
		}

		TEST_CASE("a moved document keeps its values alive")
		{
			// JsonValue points into the document, and the loaders all take one
			// by value out of read_json_file, so the move had better not be the
			// thing that dangles them.
			const ScratchFile file("document.json", R"({ "name": "kept" })");

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
			const ScratchFile file("document.json", text);

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
			const ScratchFile file("document.json", R"({ "a": 1, )");
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

		TEST_CASE("raw NUL input is refused through both JSON entry points")
		{
			struct RawNulCase
			{
				std::string name;
				std::string text;
				size_t offset;
			};
			const RawNulCase cases[] = {
				{ "root then NUL and suffix",
					std::string(R"({"a":1})") + '\0' + "junk", 7 },
				{ "root then physical terminal NUL",
					std::string(R"({"a":1})") + '\0', 7 },
				{ "whitespace then NUL", std::string(" \t\r\n") + '\0', 4 },
				{ "raw NUL inside a quoted value",
					std::string(R"({"a":"x)") + '\0' + R"(y"})", 7 },
				{ "raw NUL inside a member name",
					std::string(R"({"x)") + '\0' + R"(y":1})", 3 },
				{ "first of several NULs",
					std::string("{}") + '\0' + "tail" + '\0', 2 }
			};

			for (const RawNulCase& sample : cases)
			{
				CAPTURE(sample.name);
				const std::string offset = "offset " + std::to_string(sample.offset);
				bool text_rejected = false;
				try
				{
					parse_json(sample.text, "raw-nul.json");
				}
				catch (const std::runtime_error& error)
				{
					text_rejected = true;
					const std::string message = error.what();
					CHECK(message.find("parse_json") != std::string::npos);
					CHECK(message.find("raw-nul.json") != std::string::npos);
					CHECK(message.find("NUL") != std::string::npos);
					CHECK(message.find(offset) != std::string::npos);
				}
				CHECK(text_rejected);

				const ScratchFile file("raw-nul.json", sample.text);
				bool file_rejected = false;
				try
				{
					read_json_file(file.path().c_str());
				}
				catch (const std::runtime_error& error)
				{
					file_rejected = true;
					const std::string message = error.what();
					CHECK(message.find("read_json_file") != std::string::npos);
					CHECK(message.find(file.path()) != std::string::npos);
					CHECK(message.find("NUL") != std::string::npos);
					CHECK(message.find(offset) != std::string::npos);
				}
				CHECK(file_rejected);
			}
		}

		TEST_CASE("escaped NUL strings and member names remain valid JSON")
		{
			const std::string text = R"({"a":"x\u0000y","x\u0000y":"kept"})";
			const std::string decoded("x\0y", 3);
			const std::vector<std::string> expected_keys = { "a", decoded };
			const ScratchFile file("escaped-nul.json", text);
			const JsonDocument from_text = parse_json(text, "escaped-nul.json");
			const JsonDocument from_file = read_json_file(file.path().c_str());

			CHECK(from_text.root().string("a") == decoded);
			CHECK(from_file.root().string("a") == decoded);
			CHECK(from_text.root().keys() == expected_keys);
			CHECK(from_file.root().keys() == expected_keys);
		}

		TEST_CASE("trailing JSON whitespace remains valid through both entry points")
		{
			const std::string text = std::string(R"({"a":7})") + " \t\r\n";
			const ScratchFile file("whitespace.json", text);
			const JsonDocument from_text = parse_json(text, "whitespace.json");
			const JsonDocument from_file = read_json_file(file.path().c_str());

			CHECK(from_text.root().integer("a") == 7);
			CHECK(from_file.root().integer("a") == 7);
		}

		TEST_CASE("parse_json checks only the supplied nonterminated view")
		{
			const std::string record = R"({"a":7})";
			const std::string prefix = std::string("outside") + '\0';
			const std::string storage = prefix + record + "junk" + '\0' + "after";
			const std::string_view view(storage.data() + prefix.size(), record.size());
			const JsonDocument document = parse_json(view, "subspan.json");

			CHECK(document.root().integer("a") == 7);
			CHECK(document.source_path() == "subspan.json");
		}

		TEST_CASE("both doors agree on their BOM parsing behavior")
		{
			// Keep the parser's existing BOM behavior: this comparison does not
			// select a different acceptance policy for either entry point.
			const std::string text("\xEF\xBB\xBF{ \"a\": 1 }");
			const ScratchFile file("document.json", text);
			const std::string from_file = outcome_of(
				[&file] { return read_json_file(file.path().c_str()); });
			const std::string from_text = outcome_of(
				[&text] { return parse_json(text, "edge.json"); });

			CHECK(from_file == from_text);
		}
	}
}


TEST_CASE("JSON strings retain embedded NUL while identifier access rejects it")
{
	const ScratchFile file("document.json", R"({"name":"asset\u0000other","names":["asset\u0000other"]})");
	const JsonDocument document = read_json_file(file.path().c_str());
	CHECK(document.root().string("name") == std::string("asset\0other", 11));
	CHECK(document.root().array("names").at(0).as_string() == std::string("asset\0other", 11));
	CHECK_THROWS_WITH_AS(document.root().identifier("name"),
		doctest::Contains("name contains an embedded NUL"), std::runtime_error);
	CHECK_THROWS_WITH_AS(document.root().array("names").at(0).as_identifier(),
		doctest::Contains("names[0] contains an embedded NUL"), std::runtime_error);
}
