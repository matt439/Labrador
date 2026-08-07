#include "engine/assets/sprite_sheet_loader.h"
#include "engine/assets/json_loader.h"

using namespace MattMath;
using namespace rapidjson;

namespace
{
	NameTable<SpriteFrame> decode_sprite_frames(const Value& json)
	{
		NameTable<SpriteFrame> sprite_frames("sprite frame");
		for (auto& frame : json.GetArray())
		{
			std::string name = frame["name"].GetString();
			Vector2F origin = Vector2F::ZERO;
			bool rotated = false;
			RectangleI source_rectangle(
				frame["position"]["x"].GetInt(),
				frame["position"]["y"].GetInt(),
				frame["size"]["w"].GetInt(),
				frame["size"]["h"].GetInt());
			if (frame.HasMember("origin"))
			{
				origin.x = frame["origin"]["x"].GetFloat();
				origin.y = frame["origin"]["y"].GetFloat();
			}
			if (frame.HasMember("rotated"))
			{
				rotated = frame["rotated"].GetBool();
			}
			sprite_frames.add(name, SpriteFrame(source_rectangle, origin, rotated));
		}
		return sprite_frames;
	}

	NameTable<AnimationStrip> decode_animation_strips(const Value& json)
	{
		NameTable<AnimationStrip> animation_strips("animation strip");
		for (auto& strip : json.GetArray())
		{
			std::string name = strip["name"].GetString();
			auto first_frame = RectangleI(
				strip["first_frame"]["x"].GetInt(),
				strip["first_frame"]["y"].GetInt(),
				strip["first_frame"]["w"].GetInt(),
				strip["first_frame"]["h"].GetInt());
			int frame_count = strip["frame_count"].GetInt();
			float frame_time = strip["frame_time"].GetFloat();
			bool looping = strip["looping"].GetBool();
			animation_strips.add(name, AnimationStrip(
				first_frame, frame_count, frame_time, looping));
		}
		return animation_strips;
	}
}

std::unique_ptr<SpriteSheet> sprite_sheet_loader::load(const char* json_path,
	ID3D11ShaderResourceView* texture)
{
	const Document doc = json_loader::parse_file(json_path);

	return std::make_unique<SpriteSheet>(texture,
		decode_sprite_frames(doc["sprite_frames"]),
		decode_animation_strips(doc["animation_strips"]));
}
