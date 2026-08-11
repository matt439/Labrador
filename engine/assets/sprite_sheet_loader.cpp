#include "engine/assets/sprite_sheet_loader.h"
#include "engine/assets/json.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"

#include <memory>

using namespace mattmath;

namespace artattack
{
	namespace
	{
		NameTable<SpriteFrame> decode_sprite_frames(const JsonValue& json)
		{
			NameTable<SpriteFrame> sprite_frames("sprite frame");
			for (size_t index = 0; index < json.size(); ++index)
			{
				const JsonValue frame = json.at(index);
				const JsonValue position = frame.object("position");
				const JsonValue size = frame.object("size");

				const RectangleI source_rectangle(
					position.integer("x"),
					position.integer("y"),
					size.integer("w"),
					size.integer("h"));

				// A frame without an origin draws from its top-left corner, and
				// most of them do - the sheet only says so where it differs.
				Vector2F origin = Vector2F::ZERO;
				if (frame.has("origin"))
				{
					const JsonValue origin_json = frame.object("origin");
					origin.x = origin_json.number("x");
					origin.y = origin_json.number("y");
				}

				const bool rotated =
					frame.has("rotated") && frame.boolean("rotated");

				sprite_frames.add(frame.string("name"),
					SpriteFrame(source_rectangle, origin, rotated));
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
