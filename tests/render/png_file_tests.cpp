#include <doctest/doctest.h>

#include "engine/render/png_file.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"
#include "tests/scratch_file.h"

#include <lodepng.h>

#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

// The PNG reader, headless: files made here with lodepng's encoder in each
// colour type a paint program saves, and read back through the engine.
namespace
{
	using namespace labrador;

	std::vector<unsigned char> encode(const std::vector<unsigned char>& pixels,
		unsigned width, unsigned height, LodePNGColorType type,
		unsigned bit_depth)
	{
		std::vector<unsigned char> png;
		const unsigned failed =
			lodepng::encode(png, pixels, width, height, type, bit_depth);
		INFO(lodepng_error_text(failed));
		REQUIRE(failed == 0);
		return png;
	}

	std::string message_of(const std::vector<unsigned char>& bytes,
		const std::string& context)
	{
		std::string message;
		try
		{
			std::ignore = decode_png(bytes, context);
		}
		catch (const std::runtime_error& error)
		{
			message = error.what();
		}
		CHECK_FALSE(message.empty());
		return message;
	}
}

TEST_CASE("an opaque PNG reads as one level of r8g8b8a8 with its colours unchanged")
{
	const std::vector<unsigned char> pixels = {
		255, 0, 0, 255,   0, 255, 0, 255,   0, 0, 255, 255,
		10, 20, 30, 255,  40, 50, 60, 255,  70, 80, 90, 255,
	};
	const labrador_tests::ScratchFile file("opaque.png",
		encode(pixels, 3, 2, LCT_RGBA, 8));

	const TextureData texture = read_png_file(file.path());
	CHECK(texture.width == 3);
	CHECK(texture.height == 2);
	CHECK(texture.format == TextureFormat::r8g8b8a8_unorm);
	REQUIRE(texture.levels.size() == 1);
	CHECK(texture.levels[0].width == 3);
	CHECK(texture.levels[0].height == 2);
	CHECK(texture.levels[0].stride == 12);
	CHECK(texture.levels[0].rows == 2);
	CHECK(texture.levels[0].offset == 0);
	CHECK(texture.levels[0].size == pixels.size());
	CHECK(texture.pixels == pixels);
}

TEST_CASE("each colour channel is multiplied by its alpha, rounded to nearest")
{
	const std::vector<unsigned char> pixels = {
		200, 100, 50, 128,
		255, 255, 255, 0,
		255, 128, 1, 255,
		255, 255, 255, 1,
		3, 2, 1, 254,
	};
	const TextureData texture =
		decode_png(encode(pixels, 5, 1, LCT_RGBA, 8), "translucent");

	const std::vector<unsigned char> expected = {
		100, 50, 25, 128,
		0, 0, 0, 0,
		255, 128, 1, 255,
		1, 1, 1, 1,
		3, 2, 1, 254,
	};
	CHECK(texture.pixels == expected);

	// No channel ever exceeds its alpha, which is what keeps the
	// premultiplied blend from brightening an edge.
	for (size_t pixel = 0; pixel < texture.pixels.size(); pixel += 4)
	{
		for (size_t channel = 0; channel < 3; ++channel)
		{
			CHECK(texture.pixels[pixel + channel] <= texture.pixels[pixel + 3]);
		}
	}
}

TEST_CASE("every colour type a paint program saves comes out as 8-bit RGBA")
{
	SUBCASE("RGB with no alpha is opaque")
	{
		const TextureData texture = decode_png(
			encode({ 1, 2, 3, 4, 5, 6 }, 2, 1, LCT_RGB, 8), "rgb");
		CHECK(texture.pixels ==
			std::vector<unsigned char>{ 1, 2, 3, 255, 4, 5, 6, 255 });
	}
	SUBCASE("grey is spread across the three channels")
	{
		const TextureData texture =
			decode_png(encode({ 0, 77, 255 }, 3, 1, LCT_GREY, 8), "grey");
		CHECK(texture.pixels == std::vector<unsigned char>{
			0, 0, 0, 255, 77, 77, 77, 255, 255, 255, 255, 255 });
	}
	SUBCASE("16 bits a channel keeps the high byte")
	{
		// Big-endian, as PNG stores it: 0x12ab, 0x3400, 0xff00, 0xffff.
		const TextureData texture = decode_png(encode(
			{ 0x12, 0xab, 0x34, 0x00, 0xff, 0x00, 0xff, 0xff }, 1, 1,
			LCT_RGBA, 16), "sixteen");
		CHECK(texture.pixels ==
			std::vector<unsigned char>{ 0x12, 0x34, 0xff, 0xff });
	}
	SUBCASE("a palette with transparency is premultiplied like any other")
	{
		lodepng::State state;
		state.info_raw.colortype = LCT_RGBA;
		state.info_png.color.colortype = LCT_PALETTE;
		state.info_png.color.bitdepth = 8;
		state.encoder.auto_convert = 0;
		REQUIRE(lodepng_palette_add(&state.info_png.color,
			200, 100, 50, 128) == 0);
		REQUIRE(lodepng_palette_add(&state.info_png.color,
			9, 8, 7, 255) == 0);
		std::vector<unsigned char> png;
		const unsigned failed = lodepng::encode(png,
			std::vector<unsigned char>{ 200, 100, 50, 128, 9, 8, 7, 255 },
			2, 1, state);
		INFO(lodepng_error_text(failed));
		REQUIRE(failed == 0);

		const TextureData texture = decode_png(png, "palette");
		CHECK(texture.pixels == std::vector<unsigned char>{
			100, 50, 25, 128, 9, 8, 7, 255 });
	}
}

TEST_CASE("a file that is not a PNG is refused naming the file and the reason")
{
	const std::string message =
		message_of({ 'D', 'D', 'S', ' ', 0, 0, 0, 0 }, "art/hero.png");
	CHECK(message.find("art/hero.png") != std::string::npos);
	CHECK(message.find("not a PNG") != std::string::npos);

	// A real PNG cut short is refused too, rather than drawn half-empty.
	std::vector<unsigned char> cut =
		encode(std::vector<unsigned char>(4 * 16 * 16, 99), 16, 16, LCT_RGBA, 8);
	cut.resize(cut.size() / 2);
	CHECK(message_of(cut, "cut.png").find("cut.png") != std::string::npos);
}

namespace
{
	// A one-pixel PNG whose header claims `width` by `height`, with the
	// header's checksum recomputed so only the size is wrong: what a hostile
	// or corrupt file looks like to the reader.
	std::vector<unsigned char> claiming(unsigned width, unsigned height)
	{
		std::vector<unsigned char> png = encode(
			std::vector<unsigned char>{ 1, 2, 3, 255 }, 1, 1, LCT_RGBA, 8);
		REQUIRE(png.size() > 33);

		// The signature is eight bytes; IHDR's length and type follow, then
		// its width and height, big-endian.
		const auto put = [&png](size_t at, unsigned value)
		{
			png[at] = static_cast<unsigned char>(value >> 24);
			png[at + 1] = static_cast<unsigned char>(value >> 16);
			png[at + 2] = static_cast<unsigned char>(value >> 8);
			png[at + 3] = static_cast<unsigned char>(value);
		};
		put(16, width);
		put(20, height);
		lodepng_chunk_generate_crc(png.data() + 8);
		return png;
	}
}

TEST_CASE("a PNG header beyond max_texture_dimension is refused before decoding")
{
	const unsigned limit = static_cast<unsigned>(max_texture_dimension);
	const std::vector<std::tuple<unsigned, unsigned>> sizes = {
		{ 40000u, 40000u }, { limit + 1u, 1u }, { 1u, limit + 1u },
		{ 0xffffffffu, 0xffffffffu },
	};
	for (const std::tuple<unsigned, unsigned>& size : sizes)
	{
		CAPTURE(std::get<0>(size));
		CAPTURE(std::get<1>(size));
		const std::string message = message_of(
			claiming(std::get<0>(size), std::get<1>(size)), "art/bomb.png");
		CHECK(message.find("art/bomb.png") != std::string::npos);
		const bool named_limit =
			message.find(std::to_string(max_texture_dimension)) != std::string::npos;
		const bool refused_header = message.find("not a PNG") != std::string::npos;
		CHECK((named_limit || refused_header));
	}

	// The limit names both sides exactly when the header itself is valid.
	const std::string message = message_of(claiming(40000u, 40000u), "art/bomb.png");
	CHECK(message.find("40000 by 40000") != std::string::npos);
	CHECK(message.find(std::to_string(max_texture_dimension)) != std::string::npos);
}

TEST_CASE("a PNG exactly max_texture_dimension wide is read")
{
	const std::vector<unsigned char> pixels(
		4 * static_cast<size_t>(max_texture_dimension), 255);
	const TextureData texture = decode_png(
		encode(pixels, static_cast<unsigned>(max_texture_dimension), 1,
			LCT_RGBA, 8), "wide.png");

	CHECK(texture.width == max_texture_dimension);
	CHECK(texture.height == 1);
	CHECK(texture.pixels == pixels);
}

TEST_CASE("a PNG that is not there is out_of_range, as a missing .dds is")
{
	CHECK_THROWS_AS(std::ignore =
		read_png_file("./content/no_such_picture.png"), std::out_of_range);
}
