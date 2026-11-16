#include <doctest/doctest.h>

#include "engine/render/colour.h"

#include <ostream>

using labrador::Colour;
using labrador::colour_from_hex;
using labrador::colour_from_name;

TEST_SUITE("Colour")
{
	TEST_CASE("channels come out of the constructors as they went in")
	{
		CHECK(Colour(1.0f, 0.5f, 0.25f, 0.125f).r == doctest::Approx(1.0f));
		CHECK(Colour(1.0f, 0.5f, 0.25f, 0.125f).a == doctest::Approx(0.125f));

		// The three-argument form is opaque; the default-constructed one is
		// not, which is a trap worth a line of its own.
		CHECK(Colour(0.0f, 0.0f, 0.0f).a == doctest::Approx(1.0f));
		CHECK(Colour().a == doctest::Approx(0.0f));

		// Integers are 0-255 and divide, so the two spellings agree.
		CHECK(Colour(255, 128, 0) == Colour(1.0f, 128.0f / 255.0f, 0.0f));
	}

	TEST_CASE("every channel is clamped to [0, 1]")
	{
		const Colour over(2.0f, -1.0f, 0.5f, 7.0f);
		CHECK(over.r == doctest::Approx(1.0f));
		CHECK(over.g == doctest::Approx(0.0f));
		CHECK(over.b == doctest::Approx(0.5f));
		CHECK(over.a == doctest::Approx(1.0f));

		// The compound operators keep the invariant too.
		Colour summed = Colour::black;
		summed += Colour::white;
		summed *= 5.0f;
		CHECK(summed == Colour(1.0f, 1.0f, 1.0f, 1.0f));
	}

	TEST_CASE("a NaN channel clamps to 0, so 0/0 stays in [0, 1]")
	{
		// Black's colour channels are 0, so dividing by black is 0/0 in each.
		const Colour divided = Colour::black / Colour::black;
		CHECK(divided.r == 0.0f);
		CHECK(divided.g == 0.0f);
		CHECK(divided.b == 0.0f);
		CHECK(divided.a == 1.0f);

		Colour assigned = Colour::black;
		assigned /= Colour::black;
		CHECK(assigned == divided);

		const Colour by_zero = Colour::black / 0.0f;
		CHECK(by_zero.r == 0.0f);
		CHECK(by_zero.a == 1.0f);

		// The constructor is the same clamp, constexpr as the palette is.
		constexpr Colour from_constants(0.5f, 2.0f, -3.0f, 0.25f);
		static_assert(from_constants.g == 1.0f && from_constants.b == 0.0f);
	}

	TEST_CASE("hex parses the way the palette was built")
	{
		// The palette is 148 literal channel values, not 148 hex strings
		// parsed at start-up, so this is what pins the two spellings
		// together.
		CHECK(Colour("1e90ff") == Colour::dodger_blue);
		CHECK(Colour("2f4f4f") == Colour::dark_slate_grey);
		CHECK(Colour("ffffff") == Colour::white);

		// Eight digits carry alpha; anything else is opaque black.
		CHECK(Colour("ff000080").a == doctest::Approx(128.0f / 255.0f));
		CHECK(Colour("nonsense") == Colour::black);
	}

	TEST_CASE("the two grey spellings are the same colour")
	{
		CHECK(Colour::gray == Colour::grey);
		CHECK(Colour::dark_gray == Colour::dark_grey);
		CHECK(Colour::slate_gray == Colour::slate_grey);
	}

	TEST_CASE("colour_from_hex parses RGB bytes in either case")
	{
		CHECK(colour_from_hex("1e90ff") == Colour::dodger_blue);
		CHECK(colour_from_hex("1E90FF") == Colour::dodger_blue);
		CHECK(colour_from_hex("aBcDeF") == Colour(0xab, 0xcd, 0xef));
		CHECK(colour_from_hex("000000") == Colour::black);
		CHECK(colour_from_hex("ffffff") == Colour::white);
		CHECK(colour_from_hex("012345") == Colour(0x01, 0x23, 0x45));
	}

	TEST_CASE("colour_from_hex keeps straight alpha in the final byte")
	{
		CHECK(colour_from_hex("FF000080") == Colour(255, 0, 0, 128));
		CHECK(colour_from_hex("12345600") == Colour(0x12, 0x34, 0x56, 0));
		CHECK(colour_from_hex("123456ff") == Colour(0x12, 0x34, 0x56, 255));
		CHECK(colour_from_hex("00000000") == Colour());

		Colour assigned = Colour::white;
		assigned.set_from_hex("12345600");
		CHECK(assigned == colour_from_hex("12345600"));
		CHECK(Colour("FF000080") == colour_from_hex("FF000080"));
	}

	TEST_CASE("malformed hex reports absence while legacy parsing stays black")
	{
		const std::string_view invalid[] =
		{
			"", "f", "fff", "fffff", "fffffff", "fffffffff",
			"nonsense", "gg0000", "00000g", "0000000G", "0000000/",
			"#ffffff", "#ffffff80", "0xffffff", "0Xffffff", "0x12345678",
			" 000000", "000000 ", "00000 ", "+000000", "-000000",
			std::string_view("ff\0fff", 6), std::string_view("ffffff\0f", 8),
			std::string_view("\xff" "fffff", 6)
		};
		for (const std::string_view hex : invalid)
		{
			CAPTURE(hex);
			CHECK_FALSE(colour_from_hex(hex).has_value());
			CHECK(Colour(std::string(hex)) == Colour::black);
			Colour assigned = Colour::red;
			assigned.set_from_hex(std::string(hex));
			CHECK(assigned == Colour::black);
		}
	}

	TEST_CASE("colour_from_name answers the names content files use")
	{
		// Every colour named by game/content/levels/*.json. A rename that
		// silently dropped one of these would repaint three shipped levels.
		CHECK(colour_from_name("WHITE") == Colour::white);
		CHECK(colour_from_name("GREY") == Colour::grey);
		CHECK(colour_from_name("DARK_GREY") == Colour::dark_grey);
		CHECK(colour_from_name("DARK_SLATE_GREY") == Colour::dark_slate_grey);
		CHECK(colour_from_name("LIGHT_SKY_BLUE") == Colour::light_sky_blue);
		CHECK(colour_from_name("SADDLE_BROWN") == Colour::saddle_brown);
		CHECK(colour_from_name("TAN") == Colour::tan);
	}

	TEST_CASE("colour_from_name reports a name it does not know")
	{
		// Returning WHITE for anything unrecognised makes a typo in a level
		// file a white structure and no other symptom.
		CHECK_FALSE(colour_from_name("NOT_A_COLOUR").has_value());
		CHECK_FALSE(colour_from_name("").has_value());

		// The names are as content spells them, and only as content spells
		// them: the lookup is not case-folding.
		CHECK_FALSE(colour_from_name("white").has_value());
	}
}
