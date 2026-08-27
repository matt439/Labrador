#include <doctest/doctest.h>
#include "engine/assets/sprite_sheet_loader.h"
#include "engine/math/vector2f.h"
#include "engine/render/renderer.h"
#include "engine/render/sprite_frame.h"
#include "engine/render/sprite_sheet.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
using namespace labrador;
using namespace mattmath;

// What a sheet definition is allowed to say, and what it is not.
//
// NO DEVICE, IN EVERY CONFIGURATION. read_sprite_sheet takes a TextureHandle,
// which is an index and not a resource, so an unresolved one is enough to build
// a whole sheet and ask it what it read. Nothing here draws - the composition
// of an authored origin with a caller's is the one claim that needs a DrawList,
// and it is pinned in tests/render/null_tests.cpp instead.
//
// WHY THE TWO KEYS BELOW GET DIFFERENT ANSWERS. Both were parsed into
// SpriteFrame and then read by nothing, which is one finding and not two; the
// split is that honouring an origin is an addition on a path that already
// carries one, and honouring a rotation is the pixel contract. So the origin is
// obeyed and the rotation is refused out loud, and both halves are asserted
// here because a silent capability and a silent refusal fail the same way -
// invisibly, in somebody else's content.

namespace
{
	// A sheet definition on disk, deleted when the test leaves. The same shape
	// tests/assets/json_tests.cpp uses and deliberately not shared with it: the
	// loader takes a path, both files need four lines of RAII to give it one,
	// and a header holding those four lines would be a third file in the folder
	// earning nothing.
	class TempSheet
	{
	public:
		explicit TempSheet(const std::string& contents)
		{
			this->path_ = std::filesystem::temp_directory_path() /
				("labrador_sheet_test_" + std::to_string(next_id()) + ".json");

			std::ofstream file(this->path_, std::ios::binary);
			file << contents;
		}

		~TempSheet()
		{
			std::error_code ignored;
			std::filesystem::remove(this->path_, ignored);
		}

		TempSheet(const TempSheet&) = delete;
		TempSheet& operator=(const TempSheet&) = delete;

		std::string path() const { return this->path_.string(); }

	private:
		static int next_id()
		{
			static int id = 0;
			return ++id;
		}

		std::filesystem::path path_;
	};

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

	std::unique_ptr<SpriteSheet> load(const std::string& contents)
	{
		const TempSheet file(contents);
		return read_sprite_sheet(file.path().c_str(), TextureHandle());
	}

	// The message, not only the type. A refusal whose text does not point at
	// the frame is a refusal the person who authored the sheet cannot act on,
	// and the text is the entire product of this branch (T6).
	std::string refusal_message(const std::string& contents)
	{
		const TempSheet file(contents);
		try
		{
			read_sprite_sheet(file.path().c_str(), TextureHandle());
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

	CHECK(frame.origin() == Vector2F::ZERO);
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
