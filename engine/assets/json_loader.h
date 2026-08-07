#pragma once

#include <rapidjson/document.h>

namespace artattack
{
	// Reads the file at `path` in full and parses it as JSON.
	//
	// Throws std::runtime_error naming the path if the file cannot be opened,
	// cannot be read, or does not parse. The returned Document is therefore
	// always valid - callers never have to test HasParseError().
	rapidjson::Document read_json_file(const char* path);
}
