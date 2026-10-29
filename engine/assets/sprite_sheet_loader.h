#pragma once

#include "engine/render/sprite_sheet.h"
#include <memory>

namespace labrador
{
	// Reads the sheet definition at `json_path` and builds a SpriteSheet
	// drawing from `texture`. The frame and animation-strip tables are decoded
	// here rather than by SpriteSheet itself, so that nothing on the draw path
	// compiles a JSON parser to draw a sprite.
	//
	// Throws std::runtime_error naming the path if the file cannot be read or
	// parsed, and naming the path and the offending frame or strip if the
	// document is the wrong shape or has an unknown key (T6). This includes
	// nested position, size, origin and first-frame objects; unsupported fields
	// must be removed or corrected instead of being silently ignored.
	// A negative animation-strip `frame_count` throws std::runtime_error naming
	// the path, strip and field. Zero frames define an empty strip.
	// An unrepresentable cached strip x coordinate throws std::runtime_error
	// naming the path, strip and frame coordinate.
	//
	// AND FOR ONE THING THE DOCUMENT CAN SAY THAT IS WELL FORMED AND STILL NOT
	// DRAWABLE: a frame with `"rotated": true` is refused here, by name, rather
	// than loaded and then drawn upright. A sheet is authored content, so a key
	// the engine reads and disagrees with is a worse failure than one it
	// rejects - the .cpp carries the argument, and engine/render/sprite_frame.h
	// is why there is no member to hold the answer.
	//
	// `origin` goes the other way: it is honoured, because it costs one
	// addition on a path that already carries an origin
	// (engine/render/sprite_sheet.h).
	std::unique_ptr<SpriteSheet> read_sprite_sheet(const char* json_path,
		TextureHandle texture);
}
