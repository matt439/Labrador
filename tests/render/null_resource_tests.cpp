#include <doctest/doctest.h>

#include "engine/core/name_table.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/animation_strip.h"
#include "engine/render/colour.h"
#include "engine/render/null/recording.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/sprite_frame.h"
#include "engine/render/sprite_sheet.h"
#include "engine/render/sprite_vertex.h"
#include "engine/render/texture_object.h"
#include "tests/render/null_test_harness.h"

#include <lodepng.h>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

// The resource load path and device-resource lifetime, through the null
// Renderer.
//
// COMPILED ONLY IN THE null CONFIGURATION, for the reason null_tests.cpp
// gives, and gated for this one: these cases need a Renderer, and the null
// Renderer is the only one that needs no device. They assert what a recording
// can know - the names and slots a load fills, which file a texture is read
// from, the refusals and the messages that name the files, and that a handle
// resolved before release_device_resources still draws after the reload.
// render_resources_tests.cpp holds the table cases that need no Renderer at
// all, and resource_pixel_tests.cpp the ones that rasterise.

using namespace labrador;
using namespace render_tests::null_backend;

TEST_CASE("the load path is the real one, and its failures are the real ones")
{
	Harness harness;

	// The bytes are thrown away here, but everything before that is not: a
	// texture the manifest names and the disk has not is the same throw in
	// every configuration, and it is worth having somewhere a build machine can
	// run it.
	CHECK_THROWS_AS(load_texture_asset(harness.renderer(),
		harness.resources(), "./content/", "no_such_texture"),
		std::out_of_range);

	CHECK_THROWS_AS(std::ignore =
		harness.resources().resolve_texture("never_loaded"),
		std::out_of_range);
}

namespace
{
	// A directory of its own under the temporary one, so the texture's file
	// name is exactly `name` plus its extension, as a manifest's would be.
	class ScratchDirectory
	{
	public:
		explicit ScratchDirectory(const std::string& name) :
			path_(std::filesystem::temp_directory_path() /
				("labrador_png_" + name))
		{
			std::filesystem::remove_all(this->path_);
			REQUIRE(std::filesystem::create_directories(this->path_));
		}

		~ScratchDirectory()
		{
			std::error_code ignored;
			std::filesystem::remove_all(this->path_, ignored);
		}

		ScratchDirectory(const ScratchDirectory&) = delete;
		ScratchDirectory& operator=(const ScratchDirectory&) = delete;

		std::string directory() const
		{
			return this->path_.string() + "/";
		}

		void write(const std::string& file,
			const std::vector<unsigned char>& bytes) const
		{
			std::ofstream out(this->path_ / file, std::ios::binary);
			out.write(reinterpret_cast<const char*>(bytes.data()),
				static_cast<std::streamsize>(bytes.size()));
			REQUIRE(out.good());
		}

	private:
		std::filesystem::path path_;
	};

	std::vector<unsigned char> png_of(unsigned width, unsigned height)
	{
		std::vector<unsigned char> png;
		REQUIRE(lodepng::encode(png,
			std::vector<unsigned char>(4u * width * height, 200),
			width, height) == 0);
		return png;
	}

	std::string message_of(Harness& harness, const std::string& directory,
		const std::string& name)
	{
		std::string message;
		try
		{
			load_texture_asset(harness.renderer(), harness.resources(),
				directory, name);
		}
		catch (const std::exception& error)
		{
			message = error.what();
		}
		CHECK_FALSE(message.empty());
		return message;
	}
}

TEST_CASE("a texture may be a .png, read through the same load path as a .dds")
{
	Harness harness;
	const ScratchDirectory scratch("only");
	scratch.write("hero.png", png_of(4, 2));

	load_texture_asset(harness.renderer(), harness.resources(),
		scratch.directory(), "hero");
	const TextureHandle hero = harness.resources().resolve_texture("hero");

	// The whole of a 4x2 source spans texture coordinates 0 to 1, which is
	// how a recording shows the size the backend was given.
	DrawList list = harness.begin();
	list.draw_sprite(hero, RectangleI(0, 0, 4, 2),
		RectangleF(0.0f, 0.0f, 4.0f, 2.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	const std::vector<RecordedSprite> drawn = harness.end();
	REQUIRE(drawn.size() == 1);
	CHECK(drawn[0].texture.index() == hero.index());
	float right = 0.0f;
	float bottom = 0.0f;
	for (const SpriteVertex& corner : drawn[0].corners)
	{
		right = std::max(right, corner.texcoord.x);
		bottom = std::max(bottom, corner.texcoord.y);
	}
	CHECK(right == doctest::Approx(1.0f));
	CHECK(bottom == doctest::Approx(1.0f));
}

TEST_CASE("a texture with both a .dds and a .png is refused, naming both")
{
	Harness harness;
	const ScratchDirectory scratch("both");
	scratch.write("hero.png", png_of(2, 2));
	scratch.write("hero.dds", { 'D', 'D', 'S', ' ' });

	CHECK_THROWS_AS(load_texture_asset(harness.renderer(),
		harness.resources(), scratch.directory(), "hero"),
		std::runtime_error);
	const std::string message =
		message_of(harness, scratch.directory(), "hero");
	CHECK(message.find("hero.dds") != std::string::npos);
	CHECK(message.find("hero.png") != std::string::npos);
	CHECK_THROWS_AS(std::ignore =
		harness.resources().resolve_texture("hero"), std::out_of_range);
}

TEST_CASE("a texture with neither file is out_of_range, naming both")
{
	Harness harness;
	const ScratchDirectory scratch("neither");

	CHECK_THROWS_AS(load_texture_asset(harness.renderer(),
		harness.resources(), scratch.directory(), "hero"),
		std::out_of_range);
	const std::string message =
		message_of(harness, scratch.directory(), "hero");
	CHECK(message.find("hero.dds") != std::string::npos);
	CHECK(message.find("hero.png") != std::string::npos);
}

TEST_CASE("a .png that will not decode is refused naming its path")
{
	Harness harness;
	const ScratchDirectory scratch("broken");
	scratch.write("hero.png", { 'n', 'o', 't', ' ', 'a', ' ', 'p', 'n', 'g' });

	CHECK_THROWS_AS(load_texture_asset(harness.renderer(),
		harness.resources(), scratch.directory(), "hero"),
		std::runtime_error);
	CHECK(message_of(harness, scratch.directory(), "hero").find("hero.png") !=
		std::string::npos);
}

TEST_CASE("a handle resolved before release_device_resources draws after the reload")
{
	Harness harness;

	// TEST-GAP.md's C5 and C6 in one case. release_device_resources() is public
	// seam API with five implementations, one production caller - the shell's
	// device-lost handler - and, until this, no assertion anywhere in tests/ on
	// any backend. What it promises is stated at length in
	// engine/render/render_resources.h: the names stay, so a drawable holding a
	// handle from before the loss draws the right thing after the reload
	// refills the slot.
	//
	// AND THE PRECONDITION IS WHY THIS CASE RELOADS. The same header says this
	// call is the first half of a device loss and not a way to drop textures;
	// what follows it here is the reload, which is the shape the shell uses.
	const TextureHandle before = harness.resources().resolve_texture("quad");

	harness.resources().release_device_resources();

	// The name is still there - it is the resource behind it that went - so a
	// resolve answers, and it answers the same slot.
	const TextureHandle after_release =
		harness.resources().resolve_texture("quad");
	CHECK(after_release.index() == before.index());

	// The reload, through the same load path a device restore uses.
	load_texture_asset(harness.renderer(), harness.resources(), "./content/",
		"quad");

	const TextureHandle after_reload =
		harness.resources().resolve_texture("quad");
	CHECK(after_reload.index() == before.index());

	// And the handle taken before the release draws the reloaded texture. This
	// is the whole promise: a drawable that resolved a name at load time is
	// still correct after a device has been lost and remade under it.
	DrawList list = harness.begin();
	list.draw_sprite(before, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	const std::vector<RecordedSprite> drawn = harness.end();

	REQUIRE(drawn.size() == 1);
	CHECK(drawn[0].texture.index() == before.index());
	CHECK(drawn[0].corners[3].position.x - drawn[0].corners[0].position.x ==
		doctest::Approx(8.0f));
}

TEST_CASE("the engine's white texture is one texel, and survives a reload in its slot")
{
	Harness harness;
	add_white_texture(harness.renderer(), harness.resources());
	const TextureHandle white =
		harness.resources().resolve_texture(white_texture_name);

	// The solid-rectangle recipe: the one texel, stretched. A texcoord of 1 at
	// the far corner of a one-texel source is what a 1x1 texture gives.
	DrawList list = harness.begin();
	list.draw_sprite(white, labrador::RectangleI(0, 0, 1, 1),
		RectangleF(10.0f, 20.0f, 30.0f, 4.0f), Colour::red, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	const std::vector<RecordedSprite> drawn = harness.end();

	REQUIRE(drawn.size() == 1);
	CHECK(drawn[0].texture.index() == white.index());
	CHECK(drawn[0].corners[3].texcoord.x == doctest::Approx(1.0f));
	CHECK(drawn[0].corners[3].texcoord.y == doctest::Approx(1.0f));
	CHECK(drawn[0].corners[3].position.x - drawn[0].corners[0].position.x ==
		doctest::Approx(30.0f));

	// What the shell does around a device loss: release, then add it again.
	harness.resources().release_device_resources();
	add_white_texture(harness.renderer(), harness.resources());
	CHECK(harness.resources().resolve_texture(white_texture_name).index() ==
		white.index());

	// And the name is one no manifest can produce.
	CHECK(std::string(white_texture_name).find(':') != std::string::npos);
}

TEST_CASE("a sheet pair resolved before release_device_resources draws after the reload")
{
	// The case above, for the paired handle a TextureObject can be re-pointed
	// by (RenderResources::SheetFrame). Both halves name slots - the sheet's
	// in the sheet table, the frame's in the sheet - and a device loss empties
	// neither table, so a pair resolved at load draws the reloaded texture.
	class Sprite final : public TextureObject
	{
	public:
		using TextureObject::TextureObject;

		void draw_into(DrawList& list) const
		{
			this->TextureObject::draw(list,
				RectangleF(0.0f, 0.0f, 8.0f, 8.0f));
		}
	};

	Harness harness;

	NameTable<SpriteFrame> frames("sprite frame");
	frames.add("whole", SpriteFrame(Harness::whole()));
	harness.resources().add_sprite_sheet("sheet",
		std::make_unique<SpriteSheet>(harness.quad, std::move(frames),
			NameTable<AnimationStrip>("animation strip")));

	const RenderResources::SheetFrame pair =
		harness.resources().resolve_sheet_frame("sheet", "whole");

	harness.resources().release_device_resources();
	load_texture_asset(harness.renderer(), harness.resources(), "./content/",
		"quad");

	const Sprite sprite(pair, &harness.resources());

	DrawList list = harness.begin();
	sprite.draw_into(list);
	const std::vector<RecordedSprite> drawn = harness.end();

	REQUIRE(drawn.size() == 1);
	CHECK(drawn[0].texture.index() == harness.quad.index());
	CHECK(drawn[0].corners[3].position.x - drawn[0].corners[0].position.x ==
		doctest::Approx(8.0f));
}
