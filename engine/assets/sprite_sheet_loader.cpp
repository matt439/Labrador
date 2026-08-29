#include "engine/assets/sprite_sheet_loader.h"
#include "engine/assets/json.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"

#include <memory>
#include <stdexcept>
#include <string>

using namespace mattmath;

namespace labrador
{
	namespace
	{
		NameTable<SpriteFrame> decode_sprite_frames(const JsonValue& json)
		{
			NameTable<SpriteFrame> sprite_frames("sprite frame");
			for (size_t index = 0; index < json.size(); ++index)
			{
				const JsonValue frame = json.at(index);

				// Read before anything else so that every message below can
				// carry it. The name is required either way - NameTable::add
				// needs one - so hoisting it adds no failure, it only decides
				// which of two complaints a doubly-broken frame gets.
				const std::string name = frame.string("name");

				const JsonValue position = frame.object("position");
				const JsonValue size = frame.object("size");

				const RectangleI source_rectangle(
					position.integer("x"),
					position.integer("y"),
					size.integer("w"),
					size.integer("h"));

				// A frame without an origin draws from its top-left corner, and
				// most of them do - the sheet only says so where it differs.
				// What the two numbers mean is the seam's term rather than this
				// file's: unscaled source texels, measured from the frame's own
				// top left (engine/render/sprite_geometry.h). SpriteSheet::draw
				// is where it reaches a quad.
				Vector2F origin = Vector2F::ZERO;
				if (frame.has("origin"))
				{
					const JsonValue origin_json = frame.object("origin");
					origin.x = origin_json.number("x");
					origin.y = origin_json.number("y");
				}

				// REFUSED, NOT IGNORED, AND THAT IS EVERYTHING THIS ENGINE
				// KNOWS ABOUT PACKER ROTATION.
				//
				// This key is refused rather than parsed into a SpriteFrame
				// member nothing reads - which would have the sheet say a frame
				// is turned in the atlas, the engine draw it upright, and no
				// line anywhere say so. Quiet disagreement with a content file
				// is the T6 trap on the one path T7 says should be data, and it
				// is worse than a missing key, because the file is right and the
				// engine is wrong.
				//
				// Drawing one is not refused on cost alone. A turned frame
				// swaps its source width and height against the destination and
				// stops sampling its four corners in corner order, which is
				// engine/render/sprite_geometry.h - the pixel contract all four
				// rasterising backends are held to, and the golden set with it.
				// That is a real feature and deserves its own argument. What it
				// does not deserve is to arrive through a member that already
				// existed, so the member is gone too (render/sprite_frame.h).
				if (frame.has("rotated") && frame.boolean("rotated"))
				{
					throw std::runtime_error(frame.where() + " ('" + name +
						"') sets 'rotated', and this engine draws every frame "
						"the way it is stored: a packer turns a frame in the "
						"atlas and nothing on the draw path turns it back. "
						"Repack the sheet with rotation off.");
				}

				sprite_frames.add(name, SpriteFrame(source_rectangle, origin));
			}
			return sprite_frames;
		}

		NameTable<AnimationStrip> decode_animation_strips(const JsonValue& json)
		{
			NameTable<AnimationStrip> animation_strips("animation strip");
			for (size_t index = 0; index < json.size(); ++index)
			{
				const JsonValue strip = json.at(index);
				const JsonValue first_frame_json = strip.object("first_frame");

				const RectangleI first_frame(
					first_frame_json.integer("x"),
					first_frame_json.integer("y"),
					first_frame_json.integer("w"),
					first_frame_json.integer("h"));

				animation_strips.add(strip.string("name"), AnimationStrip(
					first_frame,
					strip.integer("frame_count"),
					strip.number("frame_time"),
					strip.boolean("looping")));
			}
			return animation_strips;
		}
	}

	std::unique_ptr<SpriteSheet> read_sprite_sheet(const char* json_path,
		TextureHandle texture)
	{
		const JsonDocument document = read_json_file(json_path);
		const JsonValue root = document.root();

		return std::make_unique<SpriteSheet>(texture,
			decode_sprite_frames(root.array("sprite_frames")),
			decode_animation_strips(root.array("animation_strips")));
	}
}
