#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace labrador
{
	class JsonDocument;

	// Reads the UTF-8 file path in full and parses it as JSON. Raw NUL bytes
	// are rejected at their byte offset; JSON `\u0000` string escapes are valid.
	//
	// Throws std::runtime_error naming the path if the file cannot be opened,
	// cannot be read, or does not parse. The returned document is therefore
	// always valid - callers never have to test for a parse error.
	JsonDocument read_json_file(const char* path);

	// Parses the bytes in `text` as JSON; the view need not end at a NUL.
	// Raw NUL bytes within the view are rejected at their byte offset; JSON
	// `\u0000` string escapes are valid. `source_name` names the source in every
	// diagnostic in place of a path - a content filename, say. The document
	// copies what it needs; neither argument is borrowed. The parse is the one
	// read_json_file makes, so the same bytes give the same document either way.
	//
	// Throws std::invalid_argument for an empty source_name, and
	// std::runtime_error naming source_name if the text does not parse.
	JsonDocument parse_json(std::string_view text, std::string source_name);

	// A borrowed view of one node inside a JsonDocument.
	//
	// Every accessor checks two things and throws naming the source, the
	// position in it and the key when either fails: that this node is the kind
	// the accessor needs, and that what it asks for is present and of the right
	// type. rapidjson's own accessors assert instead, and NDEBUG disarms the
	// assert - so a hand-edited content file that a debug build catches with a
	// dialog box is a null dereference in a Release build. Content files are
	// the one input a person types by hand, so every read of one is checked
	// (T6).
	//
	// This is a pointer, not a copy: it is valid only while the document it
	// came from is alive.
	class JsonValue
	{
	public:
		// Required members of an object.
		JsonValue object(const char* key) const;
		JsonValue array(const char* key) const;
		std::string string(const char* key) const;
		// Identifier/path strings cannot contain NULs; errors name the field.
		std::string identifier(const char* key) const;
		int integer(const char* key) const;
		float number(const char* key) const;
		bool boolean(const char* key) const;

		// True if `key` is present, whatever its type, so callers can check a
		// field a file is allowed to leave out. Throws unless this is an object.
		bool has(const char* key) const;

		// Copies this object's member names in stored order, including duplicate
		// names and the full decoded bytes of empty or NUL-containing names.
		// Throws unless this is an object. The copies survive the document's
		// destruction; this JsonValue remains a borrowed view.
		std::vector<std::string> keys() const;

		// Arrays. Both throw unless this node is one.
		size_t size() const;
		JsonValue at(size_t index) const;

		// This node read as a scalar, for array elements, which have no key.
		std::string as_string() const;
		std::string as_identifier() const;

		// Where this node is, as `'./manifest.json': assets[1]`, for a caller
		// with its own reason to reject what it read. An unknown
		// colour name or object type is a content mistake exactly like a
		// missing key, and it deserves the same sentence pointing at it (T6).
		std::string where() const;

	private:
		friend class JsonDocument;

		// `value` is the rapidjson::Value this views, as void* because that
		// type is a template instantiation which cannot be forward-declared
		// without naming its allocator - and naming it here would put rapidjson
		// back into the header this class exists to keep it out of. Every use
		// of it is one static_cast in json.cpp.
		JsonValue(const void* value, const std::string* source_path,
			std::string context);

		JsonValue child(const char* key) const;
		JsonValue element(size_t index) const;

		// `what` completes "<file>: <where in it> ..." - "is not an array",
		// "has no 'kind'".
		[[noreturn]] void fail(const std::string& what) const;

		const void* value_;

		// Owned by the document, and only ever read to name the source.
		const std::string* source_path_;

		// Where this node sits, written the way a person would point at it:
		// `assets[0].names`. Empty at the root, which prints as "the document".
		std::string context_;
	};

	// A parsed JSON document, and the owner of every JsonValue taken out of it.
	//
	// Move-only: a document is a parse of its text, and copying one would copy
	// the arena it parsed into.
	class JsonDocument
	{
	public:
		~JsonDocument();
		JsonDocument(JsonDocument&&) noexcept;
		JsonDocument& operator=(JsonDocument&&) noexcept;

		// The top of the document. Its own type is not checked here - the first
		// accessor used on it is what says which type it needed and did not
		// get.
		JsonValue root() const;

		// The path given to read_json_file, or the name given to parse_json.
		const std::string& source_path() const;

	private:
		friend JsonDocument read_json_file(const char* path);
		friend JsonDocument parse_json(std::string_view text,
			std::string source_name);

		struct Impl;
		explicit JsonDocument(std::unique_ptr<Impl> impl);

		std::unique_ptr<Impl> impl_;
	};
}
