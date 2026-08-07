#include "engine/assets/json_loader.h"

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

#include <rapidjson/error/en.h>

namespace artattack
{
	namespace
	{
		struct file_closer
		{
			void operator()(FILE* fp) const noexcept
			{
				if (fp != nullptr)
				{
					std::fclose(fp);
				}
			}
		};
		using unique_file = std::unique_ptr<FILE, file_closer>;

		// Big enough that every asset in the project is read in one or two calls,
		// small enough to sit comfortably on the stack.
		constexpr size_t READ_CHUNK_SIZE = 64 * 1024;
	}

	rapidjson::Document read_json_file(const char* path)
	{
		if (path == nullptr)
		{
			throw std::invalid_argument("read_json_file - null path");
		}

		FILE* raw_file = nullptr;
		if (fopen_s(&raw_file, path, "rb") != 0 || raw_file == nullptr)
		{
			throw std::runtime_error(
				std::string("read_json_file - cannot open '") + path + "'");
		}
		unique_file file(raw_file);

		std::string text;
		char chunk[READ_CHUNK_SIZE];
		for (;;)
		{
			const size_t read = std::fread(chunk, 1, sizeof(chunk), file.get());
			if (read == 0)
			{
				break;
			}
			text.append(chunk, read);
		}
		if (std::ferror(file.get()) != 0)
		{
			throw std::runtime_error(
				std::string("read_json_file - read error in '") + path + "'");
		}

		rapidjson::Document document;
		document.Parse(text.c_str(), text.size());
		if (document.HasParseError())
		{
			throw std::runtime_error(
				std::string("read_json_file - '") + path +
				"' is not valid JSON: " +
				rapidjson::GetParseError_En(document.GetParseError()) +
				" (offset " + std::to_string(document.GetErrorOffset()) + ")");
		}
		return document;
	}
}
