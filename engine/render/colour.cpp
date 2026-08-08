#include "engine/render/colour.h"

#include <algorithm>
#include <array>

using namespace mattmath;

namespace
{
	int hex_digit(char c)
	{
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		return -1;
	}

	bool all_hex_digits(const std::string& hex)
	{
		return std::all_of(hex.begin(), hex.end(),
			[](char c) { return hex_digit(c) >= 0; });
	}

	// The two digits at `index`, as a channel in [0, 1]. Only called once the
	// string is known to be all hex digits and long enough.
	float byte_at(const std::string& hex, size_t index)
	{
		const int value =
			hex_digit(hex[index]) * 16 + hex_digit(hex[index + 1]);
		return static_cast<float>(value) / 255.0f;
	}
}

namespace artattack
{
	Colour::Colour(const std::string& hex)
	{
		this->set_from_hex(hex);
	}
	Colour& Colour::operator=(const mattmath::Vector4F& vector)
	{
		this->r = vector.x;
		this->g = vector.y;
		this->b = vector.z;
		this->a = vector.w;
		this->clamp_colours();
		return *this;
	}
	bool Colour::operator==(const Colour& other) const
	{
		return this->r == other.r &&
			this->g == other.g &&
			this->b == other.b &&
			this->a == other.a;
	}
	bool Colour::operator!=(const Colour& other) const
	{
		return !(*this == other);
	}
	Colour& Colour::operator+=(const Colour& other)
	{
		this->r += other.r;
		this->g += other.g;
		this->b += other.b;
		this->a += other.a;
		this->clamp_colours();
		return *this;
	}
	Colour& Colour::operator-=(const Colour& other)
	{
		this->r -= other.r;
		this->g -= other.g;
		this->b -= other.b;
		this->a -= other.a;
		this->clamp_colours();
		return *this;
	}
	Colour& Colour::operator*=(const Colour& other)
	{
		this->r *= other.r;
		this->g *= other.g;
		this->b *= other.b;
		this->a *= other.a;
		this->clamp_colours();
		return *this;
	}
	Colour& Colour::operator*=(float f)
	{
		this->r *= f;
		this->g *= f;
		this->b *= f;
		this->a *= f;
		this->clamp_colours();
		return *this;
	}
	Colour& Colour::operator/=(const Colour& other)
	{
		this->r /= other.r;
		this->g /= other.g;
		this->b /= other.b;
		this->a /= other.a;
		this->clamp_colours();
		return *this;
	}
	Colour& Colour::operator/=(float f)
	{
		this->r /= f;
		this->g /= f;
		this->b /= f;
		this->a /= f;
		this->clamp_colours();
		return *this;
	}
	void Colour::set_from_hex(const std::string& hex)
	{
		// The length check alone is not the guard it looks like. This was six
		// std::stoi calls on substr temporaries, and stoi throws
		// std::invalid_argument on a string of the right length whose digits
		// are not hex - so "nonsense" is eight characters, takes the RRGGBBAA
		// branch and throws, past the else that promises opaque black. Read
		// the digits directly and there is nothing to throw.
		const size_t length = hex.length();
		const bool has_alpha = length == 8;
		if ((length != 6 && !has_alpha) || !all_hex_digits(hex))
		{
			*this = Colour::black;
			return;
		}

		this->r = byte_at(hex, 0);
		this->g = byte_at(hex, 2);
		this->b = byte_at(hex, 4);
		this->a = has_alpha ? byte_at(hex, 6) : 1.0f;
	}
	void Colour::saturate(float amount)
	{
		const float luminance =
			this->r * 0.3f + this->g * 0.59f + this->b * 0.11f;
		this->r = luminance + amount * (this->r - luminance);
		this->g = luminance + amount * (this->g - luminance);
		this->b = luminance + amount * (this->b - luminance);
		this->clamp_colours();
	}
	void Colour::brighten(float amount)
	{
		this->r += amount;
		this->g += amount;
		this->b += amount;
		this->clamp_colours();
	}
	void Colour::darken(float amount)
	{
		this->r -= amount;
		this->g -= amount;
		this->b -= amount;
		this->clamp_colours();
	}
	void Colour::invert()
	{
		this->r = 1.0f - this->r;
		this->g = 1.0f - this->g;
		this->b = 1.0f - this->b;
		this->clamp_colours();
	}
	void Colour::make_opaque()
	{
		this->a = 1.0f;
	}
	void Colour::make_transparent()
	{
		this->a = 0.0f;
	}
	void Colour::clamp_colours()
	{
		this->r = detail::clamp01(this->r);
		this->g = detail::clamp01(this->g);
		this->b = detail::clamp01(this->b);
		this->a = detail::clamp01(this->a);
	}

	Colour operator+ (const Colour& V1, const Colour& V2)
	{
		return Colour(V1.r + V2.r, V1.g + V2.g, V1.b + V2.b, V1.a + V2.a);
	}
	Colour operator- (const Colour& V1, const Colour& V2)
	{
		return Colour(V1.r - V2.r, V1.g - V2.g, V1.b - V2.b, V1.a - V2.a);
	}
	Colour operator* (const Colour& V1, const Colour& V2)
	{
		return Colour(V1.r * V2.r, V1.g * V2.g, V1.b * V2.b, V1.a * V2.a);
	}
	Colour operator* (const Colour& V, float S)
	{
		return Colour(V.r * S, V.g * S, V.b * S, V.a * S);
	}
	Colour operator/ (const Colour& V1, const Colour& V2)
	{
		return Colour(V1.r / V2.r, V1.g / V2.g, V1.b / V2.b, V1.a / V2.a);
	}
	Colour operator/ (const Colour& V, float S)
	{
		return Colour(V.r / S, V.g / S, V.b / S, V.a / S);
	}
	Colour operator* (float S, const Colour& V)
	{
		return Colour(V.r * S, V.g * S, V.b * S, V.a * S);
	}

	// The palette. Constant-initialised, so these are 2368 bytes of .rdata and
	// no start-up work; the hex strings they used to be parsed from at load
	// time are gone with them.
	const Colour Colour::alice_blue(0xf0, 0xf8, 0xff);
	const Colour Colour::antique_white(0xfa, 0xeb, 0xd7);
	const Colour Colour::aqua(0x00, 0xff, 0xff);
	const Colour Colour::aquamarine(0x7f, 0xff, 0xd4);
	const Colour Colour::azure(0xf0, 0xff, 0xff);
	const Colour Colour::beige(0xf5, 0xf5, 0xdc);
	const Colour Colour::bisque(0xff, 0xe4, 0xc4);
	const Colour Colour::black(0x00, 0x00, 0x00);
	const Colour Colour::blanched_almond(0xff, 0xeb, 0xcd);
	const Colour Colour::blue(0x00, 0x00, 0xff);
	const Colour Colour::blue_violet(0x8a, 0x2b, 0xe2);
	const Colour Colour::brown(0xa5, 0x2a, 0x2a);
	const Colour Colour::burlywood(0xde, 0xb8, 0x87);
	const Colour Colour::cadet_blue(0x5f, 0x9e, 0xa0);
	const Colour Colour::chartreuse(0x7f, 0xff, 0x00);
	const Colour Colour::chocolate(0xd2, 0x69, 0x1e);
	const Colour Colour::coral(0xff, 0x7f, 0x50);
	const Colour Colour::cornflower_blue(0x64, 0x95, 0xed);
	const Colour Colour::cornsilk(0xff, 0xf8, 0xdc);
	const Colour Colour::crimson(0xdc, 0x14, 0x3c);
	const Colour Colour::cyan(0x00, 0xff, 0xff);
	const Colour Colour::dark_blue(0x00, 0x00, 0x8b);
	const Colour Colour::dark_cyan(0x00, 0x8b, 0x8b);
	const Colour Colour::dark_goldenrod(0xb8, 0x86, 0x0b);
	const Colour Colour::dark_gray(0xa9, 0xa9, 0xa9);
	const Colour Colour::dark_green(0x00, 0x64, 0x00);
	const Colour Colour::dark_grey(0xa9, 0xa9, 0xa9);
	const Colour Colour::dark_khaki(0xbd, 0xb7, 0x6b);
	const Colour Colour::dark_magenta(0x8b, 0x00, 0x8b);
	const Colour Colour::dark_olive_green(0x55, 0x6b, 0x2f);
	const Colour Colour::dark_orange(0xff, 0x8c, 0x00);
	const Colour Colour::dark_orchid(0x99, 0x32, 0xcc);
	const Colour Colour::dark_red(0x8b, 0x00, 0x00);
	const Colour Colour::dark_salmon(0xe9, 0x96, 0x7a);
	const Colour Colour::dark_sea_green(0x8f, 0xbc, 0x8f);
	const Colour Colour::dark_slate_blue(0x48, 0x3d, 0x8b);
	const Colour Colour::dark_slate_gray(0x2f, 0x4f, 0x4f);
	const Colour Colour::dark_slate_grey(0x2f, 0x4f, 0x4f);
	const Colour Colour::dark_turquoise(0x00, 0xce, 0xd1);
	const Colour Colour::dark_violet(0x94, 0x00, 0xd3);
	const Colour Colour::deep_pink(0xff, 0x14, 0x93);
	const Colour Colour::deep_sky_blue(0x00, 0xbf, 0xff);
	const Colour Colour::dim_gray(0x69, 0x69, 0x69);
	const Colour Colour::dim_grey(0x69, 0x69, 0x69);
	const Colour Colour::dodger_blue(0x1e, 0x90, 0xff);
	const Colour Colour::fire_brick(0xb2, 0x22, 0x22);
	const Colour Colour::floral_white(0xff, 0xfa, 0xf0);
	const Colour Colour::forest_green(0x22, 0x8b, 0x22);
	const Colour Colour::fuchsia(0xff, 0x00, 0xff);
	const Colour Colour::gainsboro(0xdc, 0xdc, 0xdc);
	const Colour Colour::ghost_white(0xf8, 0xf8, 0xff);
	const Colour Colour::gold(0xff, 0xd7, 0x00);
	const Colour Colour::goldenrod(0xda, 0xa5, 0x20);
	const Colour Colour::gray(0x80, 0x80, 0x80);
	const Colour Colour::green(0x00, 0x80, 0x00);
	const Colour Colour::green_yellow(0xad, 0xff, 0x2f);
	const Colour Colour::grey(0x80, 0x80, 0x80);
	const Colour Colour::honeydew(0xf0, 0xff, 0xf0);
	const Colour Colour::hot_pink(0xff, 0x69, 0xb4);
	const Colour Colour::indian_red(0xcd, 0x5c, 0x5c);
	const Colour Colour::indigo(0x4b, 0x00, 0x82);
	const Colour Colour::ivory(0xff, 0xff, 0xf0);
	const Colour Colour::khaki(0xf0, 0xe6, 0x8c);
	const Colour Colour::lavender(0xe6, 0xe6, 0xfa);
	const Colour Colour::lavender_blush(0xff, 0xf0, 0xf5);
	const Colour Colour::lawn_green(0x7c, 0xfc, 0x00);
	const Colour Colour::lemon_chiffon(0xff, 0xfa, 0xcd);
	const Colour Colour::light_blue(0xad, 0xd8, 0xe6);
	const Colour Colour::light_coral(0xf0, 0x80, 0x80);
	const Colour Colour::light_cyan(0xe0, 0xff, 0xff);
	const Colour Colour::light_goldenrod_yellow(0xfa, 0xfa, 0xd2);
	const Colour Colour::light_gray(0xd3, 0xd3, 0xd3);
	const Colour Colour::light_green(0x90, 0xee, 0x90);
	const Colour Colour::light_grey(0xd3, 0xd3, 0xd3);
	const Colour Colour::light_pink(0xff, 0xb6, 0xc1);
	const Colour Colour::light_salmon(0xff, 0xa0, 0x7a);
	const Colour Colour::light_sea_green(0x20, 0xb2, 0xaa);
	const Colour Colour::light_sky_blue(0x87, 0xce, 0xfa);
	const Colour Colour::light_slate_gray(0x77, 0x88, 0x99);
	const Colour Colour::light_slate_grey(0x77, 0x88, 0x99);
	const Colour Colour::light_steel_blue(0xb0, 0xc4, 0xde);
	const Colour Colour::light_yellow(0xff, 0xff, 0xe0);
	const Colour Colour::lime(0x00, 0xff, 0x00);
	const Colour Colour::lime_green(0x32, 0xcd, 0x32);
	const Colour Colour::linen(0xfa, 0xf0, 0xe6);
	const Colour Colour::magenta(0xff, 0x00, 0xff);
	const Colour Colour::maroon(0x80, 0x00, 0x00);
	const Colour Colour::medium_aquamarine(0x66, 0xcd, 0xaa);
	const Colour Colour::medium_blue(0x00, 0x00, 0xcd);
	const Colour Colour::medium_orchid(0xba, 0x55, 0xd3);
	const Colour Colour::medium_purple(0x93, 0x70, 0xdb);
	const Colour Colour::medium_sea_green(0x3c, 0xb3, 0x71);
	const Colour Colour::medium_slate_blue(0x7b, 0x68, 0xee);
	const Colour Colour::medium_spring_green(0x00, 0xfa, 0x9a);
	const Colour Colour::medium_turquoise(0x48, 0xd1, 0xcc);
	const Colour Colour::medium_violet_red(0xc7, 0x15, 0x85);
	const Colour Colour::midnight_blue(0x19, 0x19, 0x70);
	const Colour Colour::mint_cream(0xf5, 0xff, 0xfa);
	const Colour Colour::misty_rose(0xff, 0xe4, 0xe1);
	const Colour Colour::moccasin(0xff, 0xe4, 0xb5);
	const Colour Colour::navajo_white(0xff, 0xde, 0xad);
	const Colour Colour::navy(0x00, 0x00, 0x80);
	const Colour Colour::old_lace(0xfd, 0xf5, 0xe6);
	const Colour Colour::olive(0x80, 0x80, 0x00);
	const Colour Colour::olive_drab(0x6b, 0x8e, 0x23);
	const Colour Colour::orange(0xff, 0xa5, 0x00);
	const Colour Colour::orange_red(0xff, 0x45, 0x00);
	const Colour Colour::orchid(0xda, 0x70, 0xd6);
	const Colour Colour::pale_goldenrod(0xee, 0xe8, 0xaa);
	const Colour Colour::pale_green(0x98, 0xfb, 0x98);
	const Colour Colour::pale_turquoise(0xaf, 0xee, 0xee);
	const Colour Colour::pale_violet_red(0xdb, 0x70, 0x93);
	const Colour Colour::papaya_whip(0xff, 0xef, 0xd5);
	const Colour Colour::peach_puff(0xff, 0xda, 0xb9);
	const Colour Colour::peru(0xcd, 0x85, 0x3f);
	const Colour Colour::pink(0xff, 0xc0, 0xcb);
	const Colour Colour::plum(0xdd, 0xa0, 0xdd);
	const Colour Colour::powder_blue(0xb0, 0xe0, 0xe6);
	const Colour Colour::purple(0x80, 0x00, 0x80);
	const Colour Colour::rebecca_purple(0x66, 0x33, 0x99);
	const Colour Colour::red(0xff, 0x00, 0x00);
	const Colour Colour::rosy_brown(0xbc, 0x8f, 0x8f);
	const Colour Colour::royal_blue(0x41, 0x69, 0xe1);
	const Colour Colour::saddle_brown(0x8b, 0x45, 0x13);
	const Colour Colour::salmon(0xfa, 0x80, 0x72);
	const Colour Colour::sandy_brown(0xf4, 0xa4, 0x60);
	const Colour Colour::sea_green(0x2e, 0x8b, 0x57);
	const Colour Colour::seashell(0xff, 0xf5, 0xee);
	const Colour Colour::sienna(0xa0, 0x52, 0x2d);
	const Colour Colour::silver(0xc0, 0xc0, 0xc0);
	const Colour Colour::sky_blue(0x87, 0xce, 0xeb);
	const Colour Colour::slate_blue(0x6a, 0x5a, 0xcd);
	const Colour Colour::slate_gray(0x70, 0x80, 0x90);
	const Colour Colour::slate_grey(0x70, 0x80, 0x90);
	const Colour Colour::snow(0xff, 0xfa, 0xfa);
	const Colour Colour::spring_green(0x00, 0xff, 0x7f);
	const Colour Colour::steel_blue(0x46, 0x82, 0xb4);
	const Colour Colour::tan(0xd2, 0xb4, 0x8c);
	const Colour Colour::teal(0x00, 0x80, 0x80);
	const Colour Colour::thistle(0xd8, 0xbf, 0xd8);
	const Colour Colour::tomato(0xff, 0x63, 0x47);
	const Colour Colour::turquoise(0x40, 0xe0, 0xd0);
	const Colour Colour::violet(0xee, 0x82, 0xee);
	const Colour Colour::wheat(0xf5, 0xde, 0xb3);
	const Colour Colour::white(0xff, 0xff, 0xff);
	const Colour Colour::white_smoke(0xf5, 0xf5, 0xf5);
	const Colour Colour::yellow(0xff, 0xff, 0x00);
	const Colour Colour::yellow_green(0x9a, 0xcd, 0x32);

	namespace
	{
		struct NamedColour
		{
			std::string_view name;
			const Colour& colour;
		};

		// The lookup content files index by name. It was a chain of 149
		// `if (name == "...")` in an `inline` function in the header, so every
		// translation unit that called it compiled its own copy of all 149
		// comparisons.
		constexpr std::array<NamedColour, 148> named_colours =
		{{
			{ "ALICE_BLUE", Colour::alice_blue },
			{ "ANTIQUE_WHITE", Colour::antique_white },
			{ "AQUA", Colour::aqua },
			{ "AQUAMARINE", Colour::aquamarine },
			{ "AZURE", Colour::azure },
			{ "BEIGE", Colour::beige },
			{ "BISQUE", Colour::bisque },
			{ "BLACK", Colour::black },
			{ "BLANCHED_ALMOND", Colour::blanched_almond },
			{ "BLUE", Colour::blue },
			{ "BLUE_VIOLET", Colour::blue_violet },
			{ "BROWN", Colour::brown },
			{ "BURLYWOOD", Colour::burlywood },
			{ "CADET_BLUE", Colour::cadet_blue },
			{ "CHARTREUSE", Colour::chartreuse },
			{ "CHOCOLATE", Colour::chocolate },
			{ "CORAL", Colour::coral },
			{ "CORNFLOWER_BLUE", Colour::cornflower_blue },
			{ "CORNSILK", Colour::cornsilk },
			{ "CRIMSON", Colour::crimson },
			{ "CYAN", Colour::cyan },
			{ "DARK_BLUE", Colour::dark_blue },
			{ "DARK_CYAN", Colour::dark_cyan },
			{ "DARK_GOLDENROD", Colour::dark_goldenrod },
			{ "DARK_GRAY", Colour::dark_gray },
			{ "DARK_GREEN", Colour::dark_green },
			{ "DARK_GREY", Colour::dark_grey },
			{ "DARK_KHAKI", Colour::dark_khaki },
			{ "DARK_MAGENTA", Colour::dark_magenta },
			{ "DARK_OLIVE_GREEN", Colour::dark_olive_green },
			{ "DARK_ORANGE", Colour::dark_orange },
			{ "DARK_ORCHID", Colour::dark_orchid },
			{ "DARK_RED", Colour::dark_red },
			{ "DARK_SALMON", Colour::dark_salmon },
			{ "DARK_SEA_GREEN", Colour::dark_sea_green },
			{ "DARK_SLATE_BLUE", Colour::dark_slate_blue },
			{ "DARK_SLATE_GRAY", Colour::dark_slate_gray },
			{ "DARK_SLATE_GREY", Colour::dark_slate_grey },
			{ "DARK_TURQUOISE", Colour::dark_turquoise },
			{ "DARK_VIOLET", Colour::dark_violet },
			{ "DEEP_PINK", Colour::deep_pink },
			{ "DEEP_SKY_BLUE", Colour::deep_sky_blue },
			{ "DIM_GRAY", Colour::dim_gray },
			{ "DIM_GREY", Colour::dim_grey },
			{ "DODGER_BLUE", Colour::dodger_blue },
			{ "FIRE_BRICK", Colour::fire_brick },
			{ "FLORAL_WHITE", Colour::floral_white },
			{ "FOREST_GREEN", Colour::forest_green },
			{ "FUCHSIA", Colour::fuchsia },
			{ "GAINSBORO", Colour::gainsboro },
			{ "GHOST_WHITE", Colour::ghost_white },
			{ "GOLD", Colour::gold },
			{ "GOLDENROD", Colour::goldenrod },
			{ "GRAY", Colour::gray },
			{ "GREEN", Colour::green },
			{ "GREEN_YELLOW", Colour::green_yellow },
			{ "GREY", Colour::grey },
			{ "HONEYDEW", Colour::honeydew },
			{ "HOT_PINK", Colour::hot_pink },
			{ "INDIAN_RED", Colour::indian_red },
			{ "INDIGO", Colour::indigo },
			{ "IVORY", Colour::ivory },
			{ "KHAKI", Colour::khaki },
			{ "LAVENDER", Colour::lavender },
			{ "LAVENDER_BLUSH", Colour::lavender_blush },
			{ "LAWN_GREEN", Colour::lawn_green },
			{ "LEMON_CHIFFON", Colour::lemon_chiffon },
			{ "LIGHT_BLUE", Colour::light_blue },
			{ "LIGHT_CORAL", Colour::light_coral },
			{ "LIGHT_CYAN", Colour::light_cyan },
			{ "LIGHT_GOLDENROD_YELLOW", Colour::light_goldenrod_yellow },
			{ "LIGHT_GRAY", Colour::light_gray },
			{ "LIGHT_GREEN", Colour::light_green },
			{ "LIGHT_GREY", Colour::light_grey },
			{ "LIGHT_PINK", Colour::light_pink },
			{ "LIGHT_SALMON", Colour::light_salmon },
			{ "LIGHT_SEA_GREEN", Colour::light_sea_green },
			{ "LIGHT_SKY_BLUE", Colour::light_sky_blue },
			{ "LIGHT_SLATE_GRAY", Colour::light_slate_gray },
			{ "LIGHT_SLATE_GREY", Colour::light_slate_grey },
			{ "LIGHT_STEEL_BLUE", Colour::light_steel_blue },
			{ "LIGHT_YELLOW", Colour::light_yellow },
			{ "LIME", Colour::lime },
			{ "LIME_GREEN", Colour::lime_green },
			{ "LINEN", Colour::linen },
			{ "MAGENTA", Colour::magenta },
			{ "MAROON", Colour::maroon },
			{ "MEDIUM_AQUAMARINE", Colour::medium_aquamarine },
			{ "MEDIUM_BLUE", Colour::medium_blue },
			{ "MEDIUM_ORCHID", Colour::medium_orchid },
			{ "MEDIUM_PURPLE", Colour::medium_purple },
			{ "MEDIUM_SEA_GREEN", Colour::medium_sea_green },
			{ "MEDIUM_SLATE_BLUE", Colour::medium_slate_blue },
			{ "MEDIUM_SPRING_GREEN", Colour::medium_spring_green },
			{ "MEDIUM_TURQUOISE", Colour::medium_turquoise },
			{ "MEDIUM_VIOLET_RED", Colour::medium_violet_red },
			{ "MIDNIGHT_BLUE", Colour::midnight_blue },
			{ "MINT_CREAM", Colour::mint_cream },
			{ "MISTY_ROSE", Colour::misty_rose },
			{ "MOCCASIN", Colour::moccasin },
			{ "NAVAJO_WHITE", Colour::navajo_white },
			{ "NAVY", Colour::navy },
			{ "OLD_LACE", Colour::old_lace },
			{ "OLIVE", Colour::olive },
			{ "OLIVE_DRAB", Colour::olive_drab },
			{ "ORANGE", Colour::orange },
			{ "ORANGE_RED", Colour::orange_red },
			{ "ORCHID", Colour::orchid },
			{ "PALE_GOLDENROD", Colour::pale_goldenrod },
			{ "PALE_GREEN", Colour::pale_green },
			{ "PALE_TURQUOISE", Colour::pale_turquoise },
			{ "PALE_VIOLET_RED", Colour::pale_violet_red },
			{ "PAPAYA_WHIP", Colour::papaya_whip },
			{ "PEACH_PUFF", Colour::peach_puff },
			{ "PERU", Colour::peru },
			{ "PINK", Colour::pink },
			{ "PLUM", Colour::plum },
			{ "POWDER_BLUE", Colour::powder_blue },
			{ "PURPLE", Colour::purple },
			{ "REBECCA_PURPLE", Colour::rebecca_purple },
			{ "RED", Colour::red },
			{ "ROSY_BROWN", Colour::rosy_brown },
			{ "ROYAL_BLUE", Colour::royal_blue },
			{ "SADDLE_BROWN", Colour::saddle_brown },
			{ "SALMON", Colour::salmon },
			{ "SANDY_BROWN", Colour::sandy_brown },
			{ "SEASHELL", Colour::seashell },
			{ "SEA_GREEN", Colour::sea_green },
			{ "SIENNA", Colour::sienna },
			{ "SILVER", Colour::silver },
			{ "SKY_BLUE", Colour::sky_blue },
			{ "SLATE_BLUE", Colour::slate_blue },
			{ "SLATE_GRAY", Colour::slate_gray },
			{ "SLATE_GREY", Colour::slate_grey },
			{ "SNOW", Colour::snow },
			{ "SPRING_GREEN", Colour::spring_green },
			{ "STEEL_BLUE", Colour::steel_blue },
			{ "TAN", Colour::tan },
			{ "TEAL", Colour::teal },
			{ "THISTLE", Colour::thistle },
			{ "TOMATO", Colour::tomato },
			{ "TURQUOISE", Colour::turquoise },
			{ "VIOLET", Colour::violet },
			{ "WHEAT", Colour::wheat },
			{ "WHITE", Colour::white },
			{ "WHITE_SMOKE", Colour::white_smoke },
			{ "YELLOW", Colour::yellow },
			{ "YELLOW_GREEN", Colour::yellow_green },
		}};
	}

	std::optional<Colour> colour_from_name(std::string_view name)
	{
		const auto found = std::find_if(named_colours.begin(),
			named_colours.end(),
			[name](const NamedColour& entry) { return entry.name == name; });

		if (found == named_colours.end())
		{
			return std::nullopt;
		}
		return found->colour;
	}
}
