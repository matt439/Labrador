#pragma once

#include <filesystem>
#include <string_view>

namespace save_example::detail
{
	// Creates a new staged file exclusively, writes all bytes and checks close.
	// Windows requests FlushFileBuffers before closing; this is not a promise
	// that directory updates or device caches survive power loss.
	void write_new_file(const std::filesystem::path& path, std::string_view bytes);

	// Publishes the staged file at the inactive generation only. The current
	// acknowledged generation is a different file and must remain untouched.
	// Throws on failure; neither the old inactive pathname nor rollback of it
	// is guaranteed by ReplaceFileW. Keep the draft and inspect both paths.
	void publish_file(const std::filesystem::path& draft,
		const std::filesystem::path& inactive);
}
