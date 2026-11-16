#pragma once

#include <string_view>

namespace labrador
{
	// The engine library's configure-time version, derived from vcpkg.json.
	// Numeric parts name its release baseline, not a compatibility promise.
	// Before 1.0, read CHANGES.md when moving between releases.
	struct Version
	{
		unsigned int major;
		unsigned int minor;
		unsigned int patch;
		// X.Y.Z for a verified release; X.Y.Z-dev otherwise. The view names
		// immutable library storage and remains valid for the process lifetime.
		std::string_view string;
		// False only for a clean exact matching tag or matching release-archive
		// metadata. An unverified archive or missing Git is development. Archive
		// metadata identifies the package, not the integrity of later edits.
		bool development;
	};

	// Returns the library's version facts. Reconfigure after changing the
	// engine pin, its tracked source or tags. Source revision is independent:
	// CMake's LABRADOR_SOURCE_REVISION remains unknown for a source archive.
	Version version();
}
