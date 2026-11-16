#include "engine/assets/sprite_sheet_loader.h"
#include "engine/assets/json.h"
#include "engine/assets/json_object_keys.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/math/vector2i.h"
#include "engine/render/texture_data.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace labrador
{
	namespace
	{
		void validate_rectangle(const RectangleI& rectangle,
			const Vector2I& texture_size, const std::string& context)
		{
			const int64_t x = rectangle.x;
			const int64_t y = rectangle.y;
			const int64_t end_x = x + static_cast<int64_t>(rectangle.width);
			const int64_t end_y = y + static_cast<int64_t>(rectangle.height);
			if (std::min(x, end_x) < 0 || std::min(y, end_y) < 0 ||
				std::max(x, end_x) > texture_size.x ||
				std::max(y, end_y) > texture_size.y)
			{
				throw std::runtime_error(context + " samples rectangle (" +
					std::to_string(rectangle.x) + ", " +
					std::to_string(rectangle.y) + ", " +
					std::to_string(rectangle.width) + ", " +
					std::to_string(rectangle.height) + ") outside its atlas of " +
					std::to_string(texture_size.x) + " by " +
					std::to_string(texture_size.y) + " texels.");
			}
		}

		NameTable<SpriteFrame> decode_sprite_frames(const JsonValue& json,
			const Vector2I& texture_size)
		{
			NameTable<SpriteFrame> sprite_frames("sprite frame");
			for (size_t index = 0; index < json.size(); ++index)
			{
				const JsonValue frame = json.at(index);

				// Read before anything else so that every message below can
				// carry it. The name is required either way - NameTable::add
				// needs one - so hoisting it adds no failure, it only decides
				// which of two complaints a doubly-broken frame gets.
				const std::string name = frame.identifier("name");
				detail::reject_unknown_keys(frame, { "name", "position", "size", "origin", "rotated" });

				const JsonValue position = frame.object("position");
				const JsonValue size = frame.object("size");
				detail::reject_unknown_keys(position, { "x", "y" });
				detail::reject_unknown_keys(size, { "w", "h" });

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
				Vector2F origin = Vector2F::zero;
				if (frame.has("origin"))
				{
					const JsonValue origin_json = frame.object("origin");
					detail::reject_unknown_keys(origin_json, { "x", "y" });
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
				// That is a real feature that deserves its own argument, and
				// SpriteFrame has no member to carry it
				// (engine/render/sprite_frame.h).
				if (frame.has("rotated") && frame.boolean("rotated"))
				{
					throw std::runtime_error(frame.where() + " ('" + name +
						"') sets 'rotated', and this engine draws every frame "
						"the way it is stored: a packer turns a frame in the "
						"atlas and nothing on the draw path turns it back. "
						"Repack the sheet with rotation off.");
				}

				validate_rectangle(source_rectangle, texture_size,
					frame.where() + " ('" + name + "')");
				sprite_frames.add(name, SpriteFrame(source_rectangle, origin));
			}
			return sprite_frames;
		}

		NameTable<AnimationStrip> decode_animation_strips(const JsonValue& json,
			const Vector2I& texture_size)
		{
			NameTable<AnimationStrip> animation_strips("animation strip");
			for (size_t index = 0; index < json.size(); ++index)
			{
				const JsonValue strip = json.at(index);
				detail::reject_unknown_keys(strip, { "name", "first_frame", "frame_count", "frame_time", "looping" });
				const JsonValue first_frame_json = strip.object("first_frame");
				detail::reject_unknown_keys(first_frame_json, { "x", "y", "w", "h" });

				const RectangleI first_frame(
					first_frame_json.integer("x"),
					first_frame_json.integer("y"),
					first_frame_json.integer("w"),
					first_frame_json.integer("h"));

				const std::string name = strip.identifier("name");
				const int frame_count = strip.integer("frame_count");
				// Refuse before the signed count becomes a vector capacity.
				if (frame_count < 0)
				{
					throw std::runtime_error(strip.where() + " ('" + name +
						"') has negative 'frame_count' " +
						std::to_string(frame_count) + ".");
				}

				// The strip caches a rectangle per frame, so the count is an
				// allocation the file chooses. Its frames sit side by side in
				// one row of an atlas, each at least one texel wide, and no
				// atlas is wider than max_texture_dimension - which bounds the
				// count without knowing this sheet's texture.
				if (frame_count > max_texture_dimension)
				{
					throw std::runtime_error(strip.where() + " ('" + name +
						"') has 'frame_count' " + std::to_string(frame_count) +
						", more frames than fit side by side in the widest "
						"texture (" + std::to_string(max_texture_dimension) +
						" texels).");
				}
				if (frame_count > 0 &&
					(first_frame.width == 0 || first_frame.height == 0))
				{
					throw std::runtime_error(strip.where() + " ('" + name +
						"') has 'frame_count' " + std::to_string(frame_count) +
						" and a 'first_frame' of " +
						std::to_string(first_frame.width) + " by " +
						std::to_string(first_frame.height) +
						" texels: a frame with no area draws nothing.");
				}

				try
				{
					AnimationStrip animation(
						first_frame,
						frame_count,
						strip.number("frame_time"),
						strip.boolean("looping"));
					for (int frame_index = 0; frame_index < frame_count; ++frame_index)
					{
						validate_rectangle(animation.frame_rect(frame_index),
							texture_size, strip.where() + " ('" + name +
							"') frame " + std::to_string(frame_index));
					}
					animation_strips.add(name, std::move(animation));
				}
				catch (const std::out_of_range& error)
				{
					throw std::runtime_error(strip.where() + " ('" + name +
						"'): " + error.what());
				}
			}
			return animation_strips;
		}
	}

	std::unique_ptr<SpriteSheet> read_sprite_sheet(const char* json_path,
		TextureHandle texture, const Vector2I& texture_size)
	{
		if (texture_size.x <= 0 || texture_size.y <= 0)
		{
			throw std::invalid_argument(std::string(json_path) +
				": sprite-sheet atlas dimensions must be positive.");
		}
		const JsonDocument document = read_json_file(json_path);
		const JsonValue root = document.root();
		detail::reject_unknown_keys(root, { "sprite_frames", "animation_strips" });

		return std::make_unique<SpriteSheet>(texture,
			decode_sprite_frames(root.array("sprite_frames"), texture_size),
			decode_animation_strips(root.array("animation_strips"), texture_size));
	}
}
