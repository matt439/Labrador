#include <doctest/doctest.h>

#include "engine/render/colour.h"

using labrador::Colour;
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

		Colour brightened = Colour::black;
		brightened.brighten(5.0f);
		CHECK(brightened == Colour(1.0f, 1.0f, 1.0f, 1.0f));
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
