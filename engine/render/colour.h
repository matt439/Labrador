#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace labrador
{
	namespace detail
	{
		// Colour's constructors clamp, and they are constexpr so that the
		// palette below costs no initialisation at all. mattmath::clamp is
		// neither, and making it so would be a change to a library this one
		// only borrows from.
		constexpr float clamp01(float value)
		{
			return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
		}
	}

	// An RGBA colour, each channel clamped to [0, 1].
	//
	// NOT IN MattMath, whose job is shapes and vectors and which is documented
	// as depending on nothing. A colour is not geometry - it is what a renderer
	// tints a sprite with - so it lives here, and so does the palette.
	struct Colour
	{
		float r = 0.0f;
		float g = 0.0f;
		float b = 0.0f;
		float a = 0.0f;

		constexpr Colour() = default;
		constexpr Colour(const Colour&) = default;
		constexpr Colour(float r, float g, float b)
			: Colour(r, g, b, 1.0f) {}
		constexpr Colour(float r, float g, float b, float a)
			: r(detail::clamp01(r)), g(detail::clamp01(g)),
			  b(detail::clamp01(b)), a(detail::clamp01(a)) {}
		constexpr Colour(int r, int g, int b, int a = 255)
			: Colour(static_cast<float>(r) / 255.0f,
				static_cast<float>(g) / 255.0f,
				static_cast<float>(b) / 255.0f,
				static_cast<float>(a) / 255.0f) {}
		explicit Colour(const std::string& hex);

		Colour& operator=(const Colour&) = default;

		bool operator==(const Colour& other) const;
		bool operator!=(const Colour& other) const;

		Colour& operator+=(const Colour& other);
		Colour& operator-=(const Colour& other);
		Colour& operator*=(const Colour& other);
		Colour& operator*=(float f);
		Colour& operator/=(const Colour& other);
		Colour& operator/=(float f);

		// Gone with the move, and none of them had a caller: red()/green()/
		// blue()/alpha() and their four setters, which duplicated the public
		// fields; set(r, g, b, a) and set_from_int_rgba(r, g, b, a), which
		// duplicated the constructors. The names were the tell - the four
		// channels cannot be spelled as parameters without shadowing either
		// the fields or, now, the palette below. The fields are the interface
		// and a whole colour is assigned, not four channels.
		void set_from_hex(const std::string& hex);

		// Scales each channel's distance from the colour's luminance: 0 is
		// fully grey, 1 leaves it alone, above 1 saturates. One function with a
		// documented range, rather than this and a desaturate(float) beside it
		// with a byte-identical body.
		void saturate(float amount);

		void brighten(float amount);
		void darken(float amount);

		void invert();

		void make_opaque();
		void make_transparent();

		void clamp_colours();

		// The CSS named colours. Declared here and defined once in colour.cpp,
		// which is the whole of the linkage fix: they were 296 `const` objects
		// at namespace scope in a header, so every translation unit that
		// included it built its own 296 - half of them std::string, all of them
		// dynamically initialised. These are constant-initialised and there is
		// one set.
		//
		// CONVENTIONS names `Colour::white` as what a constant looks like. It
		// was a SCREAMING name in a `colour_consts` namespace, which broke
		// that rule and the one-namespace-per-library rule at once.
		static const Colour alice_blue;
		static const Colour antique_white;
		static const Colour aqua;
		static const Colour aquamarine;
		static const Colour azure;
		static const Colour beige;
		static const Colour bisque;
		static const Colour black;
		static const Colour blanched_almond;
		static const Colour blue;
		static const Colour blue_violet;
		static const Colour brown;
		static const Colour burlywood;
		static const Colour cadet_blue;
		static const Colour chartreuse;
		static const Colour chocolate;
		static const Colour coral;
		static const Colour cornflower_blue;
		static const Colour cornsilk;
		static const Colour crimson;
		static const Colour cyan;
		static const Colour dark_blue;
		static const Colour dark_cyan;
		static const Colour dark_goldenrod;
		static const Colour dark_gray;
		static const Colour dark_green;
		static const Colour dark_grey;
		static const Colour dark_khaki;
		static const Colour dark_magenta;
		static const Colour dark_olive_green;
		static const Colour dark_orange;
		static const Colour dark_orchid;
		static const Colour dark_red;
		static const Colour dark_salmon;
		static const Colour dark_sea_green;
		static const Colour dark_slate_blue;
		static const Colour dark_slate_gray;
		static const Colour dark_slate_grey;
		static const Colour dark_turquoise;
		static const Colour dark_violet;
		static const Colour deep_pink;
		static const Colour deep_sky_blue;
		static const Colour dim_gray;
		static const Colour dim_grey;
		static const Colour dodger_blue;
		static const Colour fire_brick;
		static const Colour floral_white;
		static const Colour forest_green;
		static const Colour fuchsia;
		static const Colour gainsboro;
		static const Colour ghost_white;
		static const Colour gold;
		static const Colour goldenrod;
		static const Colour gray;
		static const Colour green;
		static const Colour green_yellow;
		static const Colour grey;
		static const Colour honeydew;
		static const Colour hot_pink;
		static const Colour indian_red;
		static const Colour indigo;
		static const Colour ivory;
		static const Colour khaki;
		static const Colour lavender;
		static const Colour lavender_blush;
		static const Colour lawn_green;
		static const Colour lemon_chiffon;
		static const Colour light_blue;
		static const Colour light_coral;
		static const Colour light_cyan;
		static const Colour light_goldenrod_yellow;
		static const Colour light_gray;
		static const Colour light_green;
		static const Colour light_grey;
		static const Colour light_pink;
		static const Colour light_salmon;
		static const Colour light_sea_green;
		static const Colour light_sky_blue;
		static const Colour light_slate_gray;
		static const Colour light_slate_grey;
		static const Colour light_steel_blue;
		static const Colour light_yellow;
		static const Colour lime;
		static const Colour lime_green;
		static const Colour linen;
		static const Colour magenta;
		static const Colour maroon;
		static const Colour medium_aquamarine;
		static const Colour medium_blue;
		static const Colour medium_orchid;
		static const Colour medium_purple;
		static const Colour medium_sea_green;
		static const Colour medium_slate_blue;
		static const Colour medium_spring_green;
		static const Colour medium_turquoise;
		static const Colour medium_violet_red;
		static const Colour midnight_blue;
		static const Colour mint_cream;
		static const Colour misty_rose;
		static const Colour moccasin;
		static const Colour navajo_white;
		static const Colour navy;
		static const Colour old_lace;
		static const Colour olive;
		static const Colour olive_drab;
		static const Colour orange;
		static const Colour orange_red;
		static const Colour orchid;
		static const Colour pale_goldenrod;
		static const Colour pale_green;
		static const Colour pale_turquoise;
		static const Colour pale_violet_red;
		static const Colour papaya_whip;
		static const Colour peach_puff;
		static const Colour peru;
		static const Colour pink;
		static const Colour plum;
		static const Colour powder_blue;
		static const Colour purple;
		static const Colour rebecca_purple;
		static const Colour red;
		static const Colour rosy_brown;
		static const Colour royal_blue;
		static const Colour saddle_brown;
		static const Colour salmon;
		static const Colour sandy_brown;
		static const Colour sea_green;
		static const Colour seashell;
		static const Colour sienna;
		static const Colour silver;
		static const Colour sky_blue;
		static const Colour slate_blue;
		static const Colour slate_gray;
		static const Colour slate_grey;
		static const Colour snow;
		static const Colour spring_green;
		static const Colour steel_blue;
		static const Colour tan;
		static const Colour teal;
		static const Colour thistle;
		static const Colour tomato;
		static const Colour turquoise;
		static const Colour violet;
		static const Colour wheat;
		static const Colour white;
		static const Colour white_smoke;
		static const Colour yellow;
		static const Colour yellow_green;
	};

	Colour operator+ (const Colour& V1, const Colour& V2);
	Colour operator- (const Colour& V1, const Colour& V2);
	Colour operator* (const Colour& V1, const Colour& V2);
	Colour operator* (const Colour& V, float S);
	Colour operator/ (const Colour& V1, const Colour& V2);
	Colour operator/ (const Colour& V, float S);
	Colour operator* (float S, const Colour& V);

	// Looks up one of the 148 names above, spelt as content files spell them
	// (SCREAMING_SNAKE - `game/content/levels/*.json` says "DARK_SLATE_GREY").
	// Returns nothing for a name that is not one of them; the caller decides
	// whether that is a default or an error.
	std::optional<Colour> colour_from_name(std::string_view name);
}
