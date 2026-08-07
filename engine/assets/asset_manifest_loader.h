#pragma once

#include "engine/assets/asset_manifest.h"

namespace asset_manifest_loader
{
	// Reads the manifest at `json_path`.
	//
	// The file groups assets by kind and directory, because that is how a
	// person reads them:
	//
	//     {
	//         "assets": [
	//             {
	//                 "kind": "font",
	//                 "directory": "./fonts/",
	//                 "names": [ "courier_new_bold_16" ]
	//             }
	//         ]
	//     }
	//
	// and the groups are flattened to one entry per asset, because that is how
	// a loader walks them. Order survives the flattening: assets load in the
	// order the file lists them.
	//
	// Throws std::runtime_error naming the path if the file cannot be read or
	// parsed, and naming the path and the offending group if the document is
	// the wrong shape. A manifest is hand-edited data, so every read here is
	// checked rather than assumed (T6).
	AssetManifest load(const char* json_path);
}
