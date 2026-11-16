#include <doctest/doctest.h>

#include "engine/assets/sprite_sheet_loader.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/math/vector2i.h"
#include "engine/render/animation_strip.h"
#include "engine/render/renderer.h"
#include "engine/render/sprite_frame.h"
#include "engine/render/sprite_sheet.h"
#include "engine/render/texture_data.h"
#include "tests/scratch_file.h"

#include <array>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>

using namespace labrador;
using labrador_tests::ScratchFile;

// What a sheet definition is allowed to say, and what it is not.
//
// Portable reader cases supply a TextureHandle and atlas dimensions without
// a device. Null-only loader cases also upload a real fixture and check that
// rejected definitions do not publish a partial sheet. The composition
// of an authored origin with a caller's is the one claim that needs a DrawList,
// and it is pinned in tests/render/null_tests.cpp instead.
//
// WHY THE TWO KEYS BELOW GET DIFFERENT ANSWERS. Honouring an origin is an
// addition on a path that already carries one, and honouring a rotation is
// the pixel contract. So the origin is
// obeyed and the rotation is refused out loud, and both halves are asserted
// here because a silent capability and a silent refusal fail the same way -
// invisibly, in somebody else's content.

namespace
{
	// One frame, with whatever extra keys the case under test wants after the
	// size. `extra` is written verbatim, so a case can spell a key the schema
	// does not have as easily as one it does.
	std::string sheet_of(const std::string& frames)
	{
		return R"({ "sprite_frames": [ )" + frames +
			R"( ], "animation_strips": [] })";
	}

	std::string frame_of(const std::string& name, const std::string& extra)
	{
		return R"({ "name": ")" + name + R"(", )"
			R"("position": { "x": 4, "y": 8 }, )"
			R"("size": { "w": 16, "h": 32 })" + extra + " }";
	}

	std::string strip_of(const std::string& extra, const std::string& first_frame_extra)
	{
		return R"({"name":"walk","first_frame":{"x":4,"y":8,"w":16,"h":32)" +
			first_frame_extra + R"(},"frame_count":3,"frame_time":0.125,"looping":true)" + extra + "}";
	}

	std::string sheet_with_strip(const std::string& strip)
	{
		return R"({"sprite_frames":[],"animation_strips":[)" + strip + "]}";
	}

	std::unique_ptr<SpriteSheet> load(const std::string& contents,
		const Vector2I& texture_size = Vector2I(max_texture_dimension, max_texture_dimension))
	{
		const ScratchFile file("sheet.json", contents);
		return read_sprite_sheet(file.path().c_str(), TextureHandle(), texture_size);
	}

	// The message, not only the type. A refusal whose text does not point at
	// the frame is a refusal the person who authored the sheet cannot act on,
	// and the text is the entire product of this branch (T6).
	std::string refusal_message(const std::string& contents,
		const Vector2I& texture_size = Vector2I(max_texture_dimension, max_texture_dimension))
	{
		const ScratchFile file("sheet.json", contents);
		try
		{
			read_sprite_sheet(file.path().c_str(), TextureHandle(), texture_size);
		}
		catch (const std::exception& error)
		{
			return error.what();
		}
		return "no error was thrown";
	}
}

TEST_CASE("a sheet resolves its frames by name and keeps their rectangles")
{
	const std::unique_ptr<SpriteSheet> sheet =
		load(sheet_of(frame_of("box", "")));

	const SpriteFrame& frame =
		sheet->sprite_frame(sheet->resolve_sprite_frame("box"));

	CHECK(frame.source_rectangle().x == 4);
	CHECK(frame.source_rectangle().y == 8);
	CHECK(frame.source_rectangle().width == 16);
	CHECK(frame.source_rectangle().height == 32);

	CHECK_THROWS_AS(std::ignore = sheet->resolve_sprite_frame("no_such_frame"),
		std::out_of_range);
}

TEST_CASE("an authored origin reaches the frame")
{
	// THE KEY THAT WAS READ AND DROPPED. Before this, the two numbers below
	// were parsed, stored in a member with no accessor, and never looked at
	// again - so this assertion could not have been written at all.
	const std::unique_ptr<SpriteSheet> sheet = load(sheet_of(
		frame_of("pivoted", R"(, "origin": { "x": 8.0, "y": 32.0 })")));

	const SpriteFrame& frame =
		sheet->sprite_frame(sheet->resolve_sprite_frame("pivoted"));

	// Unscaled source texels from the frame's top left, which for a 16x32 frame
	// puts this one at the middle of its bottom edge - a walking sprite's feet,
	// and the reason a sheet has the key.
	CHECK(frame.origin().x == doctest::Approx(8.0f));
	CHECK(frame.origin().y == doctest::Approx(32.0f));
}

TEST_CASE("a frame with no origin draws from its top-left corner")
{
	const std::unique_ptr<SpriteSheet> sheet =
		load(sheet_of(frame_of("plain", "")));

	// Asserted rather than assumed. Most frames in a real sheet say nothing
	// about an origin, so this is the case that governs almost every draw, and
	// SpriteSheet::draw adds it to the caller's - an origin of anything but
	// zero here would move every sprite in the tree.
	const SpriteFrame& frame =
		sheet->sprite_frame(sheet->resolve_sprite_frame("plain"));

	CHECK(frame.origin() == Vector2F::zero);
}

TEST_CASE("a frame the sheet says is rotated is refused, and named")
{
	const std::string contents = sheet_of(
		frame_of("upright", "") + ", " +
		frame_of("turned", R"(, "rotated": true)"));

	CHECK_THROWS_AS(load(contents), std::runtime_error);

	const std::string message = refusal_message(contents);

	// The three things somebody fixing the sheet needs: which file, which
	// frame, and which key. The position comes from JsonValue::where(), so the
	// sentence points exactly where a missing key would have.
	CHECK(message.find(".json") != std::string::npos);
	CHECK(message.find("sprite_frames[1]") != std::string::npos);
	CHECK(message.find("'turned'") != std::string::npos);
	CHECK(message.find("'rotated'") != std::string::npos);

	// And not the frame before it, which is the whole reason the name is in
	// there: a sheet from a packer has hundreds of these.
	CHECK(message.find("upright") == std::string::npos);
}

TEST_CASE("the refusal is of the rotation, not of the key")
{
	// A packer that emits the key for every frame and sets it where it matters
	// is the normal case, and refusing "rotated": false would reject the only
	// sheet in the only client this engine has (T6 is for broken contracts, not
	// for a file saying it did nothing).
	const std::unique_ptr<SpriteSheet> sheet = load(sheet_of(
		frame_of("upright", R"(, "rotated": false)")));

	CHECK(sheet->resolve_sprite_frame("upright").valid());
}

TEST_CASE("'rotated' is still checked for being a boolean at all")
{
	// has() answers for a key of any type, so the refusal reads the value with
	// boolean() and inherits its complaint. A sheet saying "yes" is a content
	// mistake and gets the loader's usual sentence rather than being treated as
	// absent.
	const std::string message = refusal_message(sheet_of(
		frame_of("odd", R"(, "rotated": "yes")")));

	CHECK(message.find("is not true or false") != std::string::npos);
}

TEST_CASE("sprite sheet unknown keys name the file and exact nested object")
{
	struct Refusal
	{
		std::string contents;
		const char* context;
		const char* key;
	};
	const std::array<Refusal, 9> cases = { {
		{ R"({"sprite_frames":[],"animation_strips":[],"metadata":{}})", "the document", "metadata" },
		{ sheet_of(frame_of("box", R"(,"extra":0)")), "sprite_frames[0]", "extra" },
		{ R"({"sprite_frames":[{"name":"box","position":{"x":4,"y":8,"z":0},"size":{"w":16,"h":32}}],"animation_strips":[]})",
			"sprite_frames[0].position", "z" },
		{ R"({"sprite_frames":[{"name":"box","position":{"x":4,"y":8},"size":{"w":16,"h":32,"depth":1}}],"animation_strips":[]})",
			"sprite_frames[0].size", "depth" },
		{ sheet_of(frame_of("box", R"(,"origin":{"x":1,"y":2,"z":3})")),
			"sprite_frames[0].origin", "z" },
		{ sheet_with_strip(strip_of(R"(,"frame_counts":3)", "")),
			"animation_strips[0]", "frame_counts" },
		{ sheet_with_strip(strip_of("", R"(,"extra":0)")),
			"animation_strips[0].first_frame", "extra" },
		{ sheet_of(frame_of("box", R"(,"orgin":{"x":1,"y":2})")), "sprite_frames[0]", "orgin" },
		{ sheet_of(frame_of("box", R"(,"rotatd":false)")), "sprite_frames[0]", "rotatd" },
	} };
	for (const Refusal& test : cases)
	{
		CAPTURE(test.contents);
		const ScratchFile file("sheet.json", test.contents);
		try
		{
			read_sprite_sheet(file.path().c_str(), TextureHandle(),
				Vector2I(max_texture_dimension, max_texture_dimension));
			FAIL("unknown sprite-sheet member was accepted");
		}
		catch (const std::runtime_error& error)
		{
			const std::string message(error.what());
			CHECK(message.find(file.path()) != std::string::npos);
			CHECK(message.find(test.context) != std::string::npos);
			CHECK(message.find("has unknown key '") != std::string::npos);
			CHECK(message.find(test.key) != std::string::npos);
		}
	}
}

TEST_CASE("sprite sheet strict fields preserve complete authored frames and animation strips")
{
	const std::unique_ptr<SpriteSheet> sheet = load(
		R"({"sprite_frames":[)" + frame_of("metadata", R"(,"origin":{"x":1.5,"y":2.5},"rotated":false)") +
		R"(],"animation_strips":[)" + strip_of("", "") + "]}");
	const SpriteFrame& frame = sheet->sprite_frame(sheet->resolve_sprite_frame("metadata"));
	CHECK(frame.origin() == Vector2F(1.5f, 2.5f));
	const AnimationStrip& strip = sheet->animation_strip(sheet->resolve_animation_strip("walk"));
	CHECK(strip.frame_count() == 3);
	CHECK(strip.frame_time() == doctest::Approx(0.125f));
	CHECK(strip.looping());
	CHECK(strip.frame_rect(0).x == 4);
	CHECK(strip.frame_rect(0).y == 8);
	CHECK(strip.frame_rect(0).width == 16);
	CHECK(strip.frame_rect(0).height == 32);
	CHECK(strip.frame_rect(1).x == 20);
	CHECK(strip.frame_rect(2).x == 36);
}

TEST_CASE("negative authored strip counts name their source and field")
{
	const std::array<int, 2> counts = { -1, (std::numeric_limits<int>::min)() };
	for (const int count : counts)
	{
		CAPTURE(count);
		const std::string broken =
			R"({"name":"broken","first_frame":{"x":4,"y":8,"w":16,"h":32},"frame_count":)" +
			std::to_string(count) + R"(,"frame_time":0.25,"looping":false})";
		const ScratchFile file("negative-count.json",
			sheet_with_strip(strip_of("", "") + "," + broken));
		bool rejected = false;
		try
		{
			read_sprite_sheet(file.path().c_str(), TextureHandle(),
				Vector2I(max_texture_dimension, max_texture_dimension));
		}
		catch (const std::runtime_error& error)
		{
			rejected = true;
			const std::string message = error.what();
			CHECK(message.find(file.path()) != std::string::npos);
			CHECK(message.find("animation_strips[1]") != std::string::npos);
			CHECK(message.find("'broken'") != std::string::npos);
			CHECK(message.find("frame_count") != std::string::npos);
			CHECK(message.find(std::to_string(count)) != std::string::npos);
			CHECK(message.find("'walk'") == std::string::npos);
		}
		CHECK(rejected);
	}
}

TEST_CASE("a zero-count authored strip remains empty")
{
	const std::unique_ptr<SpriteSheet> sheet = load(sheet_with_strip(
		R"({"name":"empty","first_frame":{"x":4,"y":8,"w":16,"h":32},)"
		R"("frame_count":0,"frame_time":0.5,"looping":true})"));
	const SpriteSheet::strip_handle handle = sheet->resolve_animation_strip("empty");
	CHECK(handle.valid());
	const AnimationStrip& strip = sheet->animation_strip(handle);

	CHECK(strip.frame_count() == 0);
	CHECK(strip.frame_time() == 0.5f);
	CHECK(strip.looping());
	CHECK_THROWS_AS(strip.frame_rect(0), std::out_of_range);
	CHECK_THROWS_AS(strip.frame_rect(-1), std::out_of_range);
}

TEST_CASE("positive authored strips preserve rectangles timing and looping")
{
	const std::unique_ptr<SpriteSheet> sheet = load(sheet_with_strip(
		R"({"name":"once","first_frame":{"x":4,"y":7,"w":12,"h":18},)"
		R"("frame_count":3,"frame_time":0.375,"looping":false})"));
	const AnimationStrip& strip =
		sheet->animation_strip(sheet->resolve_animation_strip("once"));

	CHECK(strip.frame_count() == 3);
	CHECK(strip.frame_time() == 0.375f);
	CHECK_FALSE(strip.looping());
	CHECK(strip.frame_rect(0) == RectangleI(4, 7, 12, 18));
	CHECK(strip.frame_rect(1) == RectangleI(16, 7, 12, 18));
	CHECK(strip.frame_rect(2) == RectangleI(28, 7, 12, 18));
	CHECK_THROWS_AS(strip.frame_rect(3), std::out_of_range);
}

namespace
{
	std::string horizontal_strip_definition(const std::string& name,
		int first_x, int width, int count, bool looping)
	{
		return R"({"name":")" + name +
			R"(","first_frame":{"x":)" + std::to_string(first_x) +
			R"(,"y":7,"w":)" + std::to_string(width) +
			R"(,"h":18},"frame_count":)" + std::to_string(count) +
			R"(,"frame_time":0.125,"looping":)" + (looping ? "true" : "false") + "}";
	}
}

TEST_CASE("representable wide strip coordinates still must fit their atlas")
{
	const int minimum = (std::numeric_limits<int>::min)();
	const int maximum = (std::numeric_limits<int>::max)();
	const std::array<std::string, 2> definitions = {
		horizontal_strip_definition("wide", minimum, 1073741824, 3, false),
		horizontal_strip_definition("backwards", maximum, -1073741825, 3, true)
	};
	for (const std::string& definition : definitions)
	{
		const std::string contents = sheet_with_strip(definition);
		CHECK_THROWS_AS(load(contents), std::runtime_error);
		const std::string message = refusal_message(contents);
		CHECK(message.find("animation_strips[0]") != std::string::npos);
		CHECK(message.find("frame 0") != std::string::npos);
		CHECK(message.find("outside its atlas") != std::string::npos);
	}
}

TEST_CASE("authored strip coordinate failures identify their file and offending strip")
{
	struct Refusal
	{
		int first_x;
		int width;
		const char* coordinate;
	};
	const int minimum = (std::numeric_limits<int>::min)();
	const int maximum = (std::numeric_limits<int>::max)();
	const std::array<Refusal, 2> cases = { {
		{ maximum - 1, 1, "2147483648" },
		{ minimum + 1, -1, "-2147483649" }
	} };
	for (const Refusal& test : cases)
	{
		CAPTURE(test.first_x);
		CAPTURE(test.width);
		const ScratchFile file("strip-coordinate.json", sheet_with_strip(
			strip_of("", "") + "," +
			horizontal_strip_definition("broken", test.first_x, test.width, 3, false)));
		bool rejected = false;
		try
		{
			read_sprite_sheet(file.path().c_str(), TextureHandle(),
				Vector2I(max_texture_dimension, max_texture_dimension));
		}
		catch (const std::runtime_error& error)
		{
			rejected = true;
			const std::string message = error.what();
			CHECK(message.find(file.path()) != std::string::npos);
			CHECK(message.find("animation_strips[1]") != std::string::npos);
			CHECK(message.find("'broken'") != std::string::npos);
			CHECK(message.find("frame 2") != std::string::npos);
			CHECK(message.find("x coordinate") != std::string::npos);
			CHECK(message.find(test.coordinate) != std::string::npos);
			CHECK(message.find("int range") != std::string::npos);
			CHECK(message.find("'walk'") == std::string::npos);
		}
		CHECK(rejected);
	}
}

TEST_CASE("zero-count authored strips ignore unused extreme horizontal geometry")
{
	struct Control
	{
		int first_x;
		int width;
	};
	const int minimum = (std::numeric_limits<int>::min)();
	const int maximum = (std::numeric_limits<int>::max)();
	const std::array<Control, 2> controls = { {
		{ maximum, minimum },
		{ minimum, maximum }
	} };
	for (const Control& control : controls)
	{
		const std::unique_ptr<SpriteSheet> sheet = load(sheet_with_strip(
			strip_of("", "") + "," + horizontal_strip_definition(
				"empty", control.first_x, control.width, 0, true)));
		const AnimationStrip& strip =
			sheet->animation_strip(sheet->resolve_animation_strip("empty"));
		CHECK(strip.frame_count() == 0);
		CHECK(strip.frame_time() == 0.125f);
		CHECK(strip.looping());
		CHECK_THROWS_AS(strip.frame_rect(0), std::out_of_range);
		CHECK_THROWS_AS(strip.frame_rect(-1), std::out_of_range);
	}
}

TEST_CASE("sprite sheet repeated keys name the file, the object and the key")
{
	struct Refusal
	{
		std::string contents;
		const char* context;
		const char* key;
	};
	const std::array<Refusal, 4> cases = { {
		{ R"({"sprite_frames":[{"name":"box","position":{"x":1,"x":2,"y":0},"size":{"w":16,"h":32}}],"animation_strips":[]})",
			"sprite_frames[0].position", "x" },
		{ sheet_of(frame_of("box", R"(,"origin":{"x":1,"y":2},"origin":{"x":3,"y":4})")),
			"sprite_frames[0]", "origin" },
		{ sheet_with_strip(strip_of(R"(,"looping":false)", "")),
			"animation_strips[0]", "looping" },
		{ sheet_with_strip(strip_of("", R"(,"w":8)")),
			"animation_strips[0].first_frame", "w" },
	} };
	for (const Refusal& test : cases)
	{
		CAPTURE(test.contents);
		const ScratchFile file("sheet.json", test.contents);
		try
		{
			read_sprite_sheet(file.path().c_str(), TextureHandle(),
				Vector2I(max_texture_dimension, max_texture_dimension));
			FAIL("a repeated sprite-sheet member was accepted");
		}
		catch (const std::runtime_error& error)
		{
			const std::string message(error.what());
			CHECK(message.find(file.path()) != std::string::npos);
			CHECK(message.find(test.context) != std::string::npos);
			CHECK(message.find(std::string("repeats key '") + test.key + "'") !=
				std::string::npos);
		}
	}
}

TEST_CASE("an authored strip count beyond the widest texture is refused before allocating")
{
	// With a zero-width frame every coordinate stays in range, so before the
	// bound this asked for 2^31 cached rectangles - 32 GB - and failed with
	// bad_alloc naming nothing.
	struct Refusal
	{
		int width;
		int height;
		int count;
		const char* reason;
	};
	const int maximum = (std::numeric_limits<int>::max)();
	const std::array<Refusal, 5> cases = { {
		{ 0, 18, maximum, "widest texture" },
		{ 1, 18, maximum, "widest texture" },
		{ 1, 18, max_texture_dimension + 1, "widest texture" },
		{ 0, 18, 3, "no area" },
		{ 12, 0, 1, "no area" },
	} };
	for (const Refusal& test : cases)
	{
		CAPTURE(test.width);
		CAPTURE(test.height);
		CAPTURE(test.count);
		const std::string broken =
			R"({"name":"broken","first_frame":{"x":0,"y":0,"w":)" +
			std::to_string(test.width) + R"(,"h":)" + std::to_string(test.height) +
			R"(},"frame_count":)" + std::to_string(test.count) +
			R"(,"frame_time":0.1,"looping":false})";
		const ScratchFile file("strip-count.json",
			sheet_with_strip(strip_of("", "") + "," + broken));
		bool rejected = false;
		try
		{
			read_sprite_sheet(file.path().c_str(), TextureHandle(),
				Vector2I(max_texture_dimension, max_texture_dimension));
		}
		catch (const std::runtime_error& error)
		{
			rejected = true;
			const std::string message = error.what();
			CHECK(message.find(file.path()) != std::string::npos);
			CHECK(message.find("animation_strips[1]") != std::string::npos);
			CHECK(message.find("'broken'") != std::string::npos);
			CHECK(message.find("frame_count") != std::string::npos);
			CHECK(message.find(std::to_string(test.count)) != std::string::npos);
			CHECK(message.find(test.reason) != std::string::npos);
			CHECK(message.find("'walk'") == std::string::npos);
		}
		CHECK(rejected);
	}
}

TEST_CASE("an authored strip as wide as the widest texture is read")
{
	const std::unique_ptr<SpriteSheet> sheet = load(sheet_with_strip(
		horizontal_strip_definition("full", 0, 1, max_texture_dimension, true)));
	const AnimationStrip& strip =
		sheet->animation_strip(sheet->resolve_animation_strip("full"));

	CHECK(strip.frame_count() == max_texture_dimension);
	CHECK(strip.frame_rect(max_texture_dimension - 1) ==
		RectangleI(max_texture_dimension - 1, 7, 1, 18));
}

namespace
{
	std::string frame_rectangle(const std::string& name, const RectangleI& rectangle)
	{
		return R"({"name":")" + name + R"(","position":{"x":)" +
			std::to_string(rectangle.x) + R"(,"y":)" +
			std::to_string(rectangle.y) + R"(},"size":{"w":)" +
			std::to_string(rectangle.width) + R"(,"h":)" +
			std::to_string(rectangle.height) + "}}";
	}
}

TEST_CASE("atlas validation preserves signed rectangles and in-bounds degenerate frames")
{
	const std::array<RectangleI, 8> rectangles = {
		RectangleI(0, 0, 64, 32), RectangleI(64, 32, -64, -32),
		RectangleI(64, 0, -16, 16), RectangleI(0, 32, 16, -16),
		RectangleI(0, 0, 0, 0), RectangleI(64, 32, 0, 0),
		RectangleI(16, 32, -16, 0), RectangleI(64, 16, 0, -16)
	};
	for (const RectangleI& rectangle : rectangles)
	{
		const std::unique_ptr<SpriteSheet> sheet = load(
			sheet_of(frame_rectangle("edge", rectangle)), Vector2I(64, 32));
		CHECK(sheet->sprite_frame(sheet->resolve_sprite_frame("edge"))
			.source_rectangle() == rectangle);
	}
}

TEST_CASE("out-of-atlas frame endpoints are refused without integer overflow")
{
	const int minimum = (std::numeric_limits<int>::min)();
	const int maximum = (std::numeric_limits<int>::max)();
	const std::array<RectangleI, 12> rectangles = {
		RectangleI(-1, 0, 2, 2), RectangleI(0, -1, 2, 2),
		RectangleI(64, 0, 1, 1), RectangleI(0, 32, 1, 1),
		RectangleI(65, 0, -1, 1), RectangleI(0, 33, 1, -1),
		RectangleI(0, 0, -1, 1), RectangleI(0, 0, 1, -1),
		RectangleI(maximum, 0, maximum, 1),
		RectangleI(0, minimum, 1, minimum),
		RectangleI(65, 0, 0, 1), RectangleI(0, 33, 1, 0)
	};
	for (const RectangleI& rectangle : rectangles)
	{
		const std::string contents = sheet_of(frame_rectangle("outside", rectangle));
		CHECK_THROWS_AS(load(contents, Vector2I(64, 32)), std::runtime_error);
		const std::string message = refusal_message(contents, Vector2I(64, 32));
		CHECK(message.find("sheet.json") != std::string::npos);
		CHECK(message.find("sprite_frames[0]") != std::string::npos);
		CHECK(message.find("'outside'") != std::string::npos);
		CHECK(message.find("outside its atlas of 64 by 32 texels") != std::string::npos);
	}
}

TEST_CASE("each expanded signed strip frame must fit its atlas")
{
	const std::unique_ptr<SpriteSheet> sheet = load(sheet_with_strip(
		horizontal_strip_definition("forward", 0, 16, 4, true) + "," +
		horizontal_strip_definition("backward", 64, -16, 4, false)), Vector2I(64, 32));
	const AnimationStrip& backward =
		sheet->animation_strip(sheet->resolve_animation_strip("backward"));
	for (int index = 0; index < 4; ++index)
	{
		CHECK(backward.frame_rect(index) == RectangleI(64 - index * 16, 7, -16, 18));
	}
	for (const int width : { 16, -16 })
	{
		const std::string contents = sheet_with_strip(horizontal_strip_definition(
			"too_long", width > 0 ? 0 : 64, width, 5, true));
		CHECK_THROWS_AS(load(contents, Vector2I(64, 32)), std::runtime_error);
		const std::string message = refusal_message(contents, Vector2I(64, 32));
		CHECK(message.find("animation_strips[0]") != std::string::npos);
		CHECK(message.find("'too_long'") != std::string::npos);
		CHECK(message.find("frame 4") != std::string::npos);
		CHECK(message.find("outside its atlas") != std::string::npos);
	}
	const std::string vertically_outside = sheet_with_strip(
		R"({"name":"high","first_frame":{"x":0,"y":32,"w":16,"h":-33},)"
		R"("frame_count":1,"frame_time":0.5,"looping":true})");
	CHECK_THROWS_AS(load(vertically_outside, Vector2I(64, 32)), std::runtime_error);
	const std::unique_ptr<SpriteSheet> upside_down = load(sheet_with_strip(
		R"({"name":"up","first_frame":{"x":64,"y":32,"w":-16,"h":-32},)"
		R"("frame_count":4,"frame_time":0.5,"looping":true})"), Vector2I(64, 32));
	const AnimationStrip& up = upside_down->animation_strip(
		upside_down->resolve_animation_strip("up"));
	CHECK(up.frame_rect(3) == RectangleI(16, 32, -16, -32));
}

TEST_CASE("sprite sheet atlas dimensions must be positive")
{
	for (const Vector2I& dimensions : {
		Vector2I(0, 32), Vector2I(64, 0), Vector2I(-1, 32), Vector2I(64, -1) })
	{
		CHECK_THROWS_AS(load(sheet_of(""), dimensions), std::invalid_argument);
	}
}
