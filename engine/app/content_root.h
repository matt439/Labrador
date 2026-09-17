#pragma once

#include "engine/assets/asset_manifest.h"

#include <string>

namespace labrador
{
	// Where a game's content is, settled once, here, in the shell.
	//
	// THE WORKING DIRECTORY IS NOT AN ANSWER. Both samples loaded
	// "./manifest.json", and the manifest's own groups name "./fonts/" and
	// "./textures/"; every one of those was resolved by the C runtime against
	// wherever the process happened to be started from. Launched from the
	// build directory it worked, because the build copies the content beside
	// the executable; launched from the repository root, a shortcut, a
	// debugger with its own idea of a start directory or a file manager, it
	// threw "cannot open './manifest.json'" before the window had drawn a
	// frame - and README said it ran from anywhere
	// (docs/review/gpt6/README.md, G6-09).
	//
	// THE POLICY IS TWO SENTENCES. A relative manifest path is relative to the
	// directory the executable was loaded from, which is where a build puts
	// the content and where an installer does too. A relative directory
	// inside the manifest is relative to the manifest, so a manifest and its
	// folders move together and the file reads the same wherever it sits.
	// An absolute path anywhere is left alone, for the game that keeps its
	// content somewhere deliberate.
	//
	// IT LIVES IN app/ AND NOT IN assets/, on purpose. The loader hands a
	// directory straight through from the file to the kind that opens it,
	// and its tests pin that it does; where the file is relative TO is a
	// question about the process, and the shell is the module that is
	// allowed to ask the machine one (application.h, default_thread_count).
	// Application::load_manifest applies both sentences; these are the
	// pieces, public so that a game with its own loading order can apply
	// them itself, and so that they are testable without a window.

	// The directory the running executable was loaded from, with a trailing
	// separator, so that a relative path appended to it is a path.
	std::string executable_directory();

	// `path` itself if it is absolute, and otherwise `directory` + `path`.
	// `directory` is expected to end in a separator, as executable_directory
	// does; one is added if it does not.
	std::string resolved_under(const std::string& directory,
		const std::string& path);

	// The same manifest with every relative `directory` resolved under the
	// directory of the file it was read from (AssetManifest::source_path),
	// so the loader opens the folders beside the manifest rather than the
	// folders beside the process. A manifest with no source path is left as
	// it is: there is nothing to anchor to.
	AssetManifest anchored_to_source(AssetManifest manifest);
}
