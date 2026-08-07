#ifndef SPRITE_SHEET_LOADER_H
#define SPRITE_SHEET_LOADER_H

#include "engine/render/sprite_sheet.h"
#include <d3d11_1.h>
#include <memory>

namespace sprite_sheet_loader
{
	// Reads the sheet definition at `json_path` and builds a SpriteSheet
	// drawing from `texture`. The frame and animation-strip tables are decoded
	// here rather than by SpriteSheet itself, so that nothing on the draw path
	// compiles a JSON parser to draw a sprite.
	//
	// Throws std::runtime_error naming the path if the file cannot be read or
	// parsed, and whatever rapidjson throws if the document is the wrong shape.
	std::unique_ptr<SpriteSheet> load(const char* json_path,
		ID3D11ShaderResourceView* texture);
}

#endif // !SPRITE_SHEET_LOADER_H
