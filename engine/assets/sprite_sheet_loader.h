#pragma once

#include "engine/render/sprite_sheet.h"
#include <memory>

namespace artattack
{
	// Reads the sheet definition at `json_path` and builds a SpriteSheet
	// drawing from `texture`. The frame and animation-strip tables are decoded
	// here rather than by SpriteSheet itself, so that nothing on the draw path
	// compiles a JSON parser to draw a sprite.
	//
	// Throws std::runtime_error naming the path if the file cannot be read or
	// parsed, and naming the path and the offending frame or strip if the
	// document is the wrong shape (T6).
	std::unique_ptr<SpriteSheet> read_sprite_sheet(const char* json_path,
		TextureHandle texture);
}
