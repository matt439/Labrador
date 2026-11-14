#include "engine/render/post_effect.h"

#include <cmath>
#include <stdexcept>
#include <string>

namespace labrador
{
	namespace
	{
		void check_finite(float value, const char* name)
		{
			if (!std::isfinite(value))
			{
				throw std::invalid_argument(std::string("CrtSettings::") +
					name + " is not a finite number.");
			}
		}

		void check_strength(float value, const char* name)
		{
			check_finite(value, name);
			if (value < 0.0f)
			{
				throw std::invalid_argument(std::string("CrtSettings::") +
					name + " is negative. Every CRT strength is zero or more, "
					"and zero turns that term off.");
			}
		}

		void check_unit(float value, const char* name)
		{
			check_strength(value, name);
			if (value > 1.0f)
			{
				throw std::invalid_argument(std::string("CrtSettings::") +
					name + " is above 1. It is a fraction from 0, off, to 1, "
					"full.");
			}
		}

		// The material's barrel, restated: centred runs -1..1 across the
		// screen. crt.hlsl's barrel() is the other copy.
		float barrel_scale(float centred_x, float centred_y, float curvature)
		{
			return 1.0f + curvature *
				(centred_x * centred_x + centred_y * centred_y);
		}
	}

	PostValues crt_values(const CrtSettings& settings,
		const Vector2F& back_buffer_size)
	{
		check_finite(settings.screen.x, "screen.x");
		check_finite(settings.screen.y, "screen.y");
		check_finite(settings.screen.width, "screen.width");
		check_finite(settings.screen.height, "screen.height");
		check_strength(settings.curvature, "curvature");
		check_strength(settings.scanline_count, "scanline_count");
		check_unit(settings.scanline_strength, "scanline_strength");
		check_unit(settings.mask_strength, "mask_strength");
		check_unit(settings.vignette, "vignette");
		check_strength(settings.aberration, "aberration");
		check_strength(settings.brightness, "brightness");

		if (!(back_buffer_size.x > 0.0f) || !(back_buffer_size.y > 0.0f))
		{
			throw std::invalid_argument("crt_values was given a back buffer "
				"with no area. Pass Renderer::back_buffer_size().");
		}
		if (!(settings.screen.width > 0.0f) || !(settings.screen.height > 0.0f))
		{
			throw std::invalid_argument("CrtSettings::screen has no area. Give "
				"the rectangle, in back-buffer pixels, the CRT is drawn over.");
		}
		if (settings.scanline_strength > 0.0f &&
			(!(settings.scanline_count > 0.0f) ||
				settings.screen.height / settings.scanline_count <
					min_pixels_per_scanline))
		{
			throw std::invalid_argument("CrtSettings::scanline_count is zero, "
				"or gives fewer than 4 output pixels a scanline at this screen "
				"height, which aliases into moire bands. With scanlines on, "
				"keep the count above zero and at most screen.height / 4.");
		}

		PostValues packed;
		std::array<float, 32>& v = packed.values;
		v[0] = settings.screen.x / back_buffer_size.x;
		v[1] = settings.screen.y / back_buffer_size.y;
		v[2] = settings.screen.width / back_buffer_size.x;
		v[3] = settings.screen.height / back_buffer_size.y;
		v[4] = settings.curvature;
		v[5] = settings.scanline_count;
		v[6] = settings.scanline_strength;
		v[7] = settings.mask_strength;
		v[8] = settings.vignette;
		v[9] = settings.aberration;
		v[10] = settings.brightness;
		return packed;
	}

	bool crt_values_are_neutral(const PostValues& values)
	{
		const std::array<float, 32>& v = values.values;
		const bool no_screen = !(v[2] > 0.0f) || !(v[3] > 0.0f);
		const bool every_term_off = v[4] == 0.0f && v[6] == 0.0f &&
			v[7] == 0.0f && v[8] == 0.0f && v[9] == 0.0f && v[10] == 1.0f;
		return no_screen || every_term_off;
	}

	std::optional<Vector2F> crt_source_position(const CrtSettings& settings,
		const Vector2F& output_pixel)
	{
		const RectangleF& screen = settings.screen;
		if (!(screen.width > 0.0f) || !(screen.height > 0.0f))
		{
			return output_pixel;
		}
		const float local_x = (output_pixel.x - screen.x) / screen.width;
		const float local_y = (output_pixel.y - screen.y) / screen.height;
		if (local_x < 0.0f || local_x > 1.0f || local_y < 0.0f ||
			local_y > 1.0f)
		{
			return output_pixel;
		}

		const float centred_x = local_x * 2.0f - 1.0f;
		const float centred_y = local_y * 2.0f - 1.0f;
		const float scale = barrel_scale(centred_x, centred_y,
			settings.curvature);
		const float curved_x = centred_x * scale;
		const float curved_y = centred_y * scale;
		if (std::fabs(curved_x) > 1.0f || std::fabs(curved_y) > 1.0f)
		{
			return std::nullopt;
		}

		return Vector2F(screen.x + (curved_x * 0.5f + 0.5f) * screen.width,
			screen.y + (curved_y * 0.5f + 0.5f) * screen.height);
	}
}
