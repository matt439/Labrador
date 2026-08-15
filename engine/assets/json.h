#pragma once

#include <cstddef>
#include <memory>
#include <string>

namespace labrador
{
	class JsonDocument;

	// Reads the file at `path` in full and parses it as JSON.
	//
	// Throws std::runtime_error naming the path if the file cannot be opened,
	// cannot be read, or does not parse. The returned document is therefore
	// always valid - callers never have to test for a parse error.
	JsonDocument read_json_file(const char* path);

	// A borrowed view of one node inside a JsonDocument.
	//
	// Every accessor checks two things and throws naming the file, the position
	// in it and the key when either fails: that this node is the kind the
	// accessor needs, and that what it asks for is present and of the right
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
		int integer(const char* key) const;
		float number(const char* key) const;
		bool boolean(const char* key) const;

		// True if `key` is present, whatever its type. The one door to a field
		// a file is allowed to leave out.
		bool has(const char* key) const;

		// Arrays. Both throw unless this node is one.
		size_t size() const;
		JsonValue at(size_t index) const;

		// This node read as a scalar, for array elements, which have no key.
		std::string as_string() const;

		// Where this node is, as `'./levels/turbulence.json': objects[17]`, for
		// a caller with its own reason to reject what it read. An unknown
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

		// Owned by the document, and only ever read to name the file.
		const std::string* source_path_;

		// Where this node sits, written the way a person would point at it:
		// `assets[0].names`. Empty at the root, which prints as "the document".
		std::string context_;
	};

	// A parsed JSON file, and the owner of every JsonValue taken out of it.
	//
	// Move-only: a document is a parse of a file, and copying one would copy
	// the arena it parsed into.
	class JsonDocument
	{
	public:
		~JsonDocument();
		JsonDocument(JsonDocument&&) noexcept;
		JsonDocument& operator=(JsonDocument&&) noexcept;

		// The top of the file. Its own type is not checked here - the first
		// accessor used on it is what says which type it needed and did not
		// get.
		JsonValue root() const;

		// The path this was read from, as it was given to read_json_file.
		const std::string& source_path() const;

	private:
		friend JsonDocument read_json_file(const char* path);

		struct Impl;
		explicit JsonDocument(std::unique_ptr<Impl> impl);

		std::unique_ptr<Impl> impl_;
	};
}
