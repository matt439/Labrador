#include "engine/render/sprite_geometry.h"

#include <cmath>

using namespace mattmath;

namespace labrador
{
	namespace
	{
		// The unit square, in the corner order sprite_geometry.h fixes.
		const Vector2F CORNERS[4] =
		{
			Vector2F(0.0f, 0.0f),
			Vector2F(1.0f, 0.0f),
			Vector2F(0.0f, 1.0f),
			Vector2F(1.0f, 1.0f),
		};

		// A FLIP IS AN INDEX, NOT A NEGATION, and this is the trick worth
		// keeping rather than reinventing. The texture coordinate of corner i
		// is read from CORNERS[i ^ flip] instead of CORNERS[i]: 1 swaps left
		// and right, 2 swaps top and bottom, 3 does both. That is why a flip
		// mirrors the texture and leaves the destination rectangle exactly
		// where it was, which is a term RenderPixelTests pins and the obvious
		// alternative - negating the destination size - gets wrong.
		int mirror_bits(SpriteFlip flip)
		{
			switch (flip)
			{
			case SpriteFlip::horizontal: return 1;
			case SpriteFlip::vertical:   return 2;
			case SpriteFlip::both:       return 3;
			case SpriteFlip::none:
			default:                     return 0;
			}
		}

		// Dividing the origin by the source size is how it becomes a fraction
		// of the destination, and a source of zero width is a division by
		// nothing. The smallest positive float stands in, which puts the origin
		// somewhere enormous and off screen - the same answer DirectXTK gave,
		// and the right one: a zero-area source draws nothing whatever the
		// origin is.
		float without_zero(float value)
		{
			// FLT_EPSILON, spelt out rather than taken from mattmath::EPSILON -
			// that one is a tolerance for comparing geometry and this is a
			// stand-in for a divisor, and the two having the same value today
			// would be a coincidence rather than a reason.
			const float SMALLEST = 1.192092896e-7f;
			return value == 0.0f ? SMALLEST : value;
		}

		// The core both entry points reach: a destination already resolved to a
		// position and a size in view pixels.
		void build_quad(const Vector2F& position,
			const Vector2F& size,
			const RectangleI& source,
			const Vector2F& texture_size,
			const Colour& tint,
			float rotation,
			const Vector2F& origin,
			SpriteFlip flip,
			SpriteVertex* corners)
		{
			const Vector2F source_size(static_cast<float>(source.width),
				static_cast<float>(source.height));

			// The origin, as a fraction of the source - and therefore of the
			// destination, whatever the two sizes are.
			const Vector2F origin_ratio(origin.x / without_zero(source_size.x),
				origin.y / without_zero(source_size.y));

			// Texels to texture coordinates. The source rectangle selects, and
			// it is in texels (RenderPixelTests); the sampler wants 0..1.
			const Vector2F uv_origin(
				static_cast<float>(source.x) / texture_size.x,
				static_cast<float>(source.y) / texture_size.y);
			const Vector2F uv_size(source_size.x / texture_size.x,
				source_size.y / texture_size.y);

			// A 2x2 rotation, and the branch is worth having: the overwhelming
			// majority of sprites in both clients are unrotated, and a sine and
			// a cosine per sprite for an angle of zero is a real cost in a
			// frame that draws thousands. What that comes to is measured now
			// rather than asserted - bench/render_bench.cpp runs the same
			// sprites at both angles and bench/main.cpp prints the ratio.
			float cosine = 1.0f;
			float sine = 0.0f;
			if (rotation != 0.0f)
			{
				cosine = std::cos(rotation);
				sine = std::sin(rotation);
			}

			const int mirror = mirror_bits(flip);

			for (int i = 0; i < 4; i++)
			{
				const Vector2F offset =
					(CORNERS[i] - origin_ratio) * size;

				corners[i].position = Vector2F(
					position.x + offset.x * cosine - offset.y * sine,
					position.y + offset.x * sine + offset.y * cosine);

				corners[i].colour = tint;

				corners[i].texcoord =
					uv_origin + CORNERS[i ^ mirror] * uv_size;
			}
		}

		// The box around the four positions build_quad would write, given
		// the origin already in view pixels rather than as a fraction of the
		// source. Same corners, same turn about `position`; the texture
		// terms play no part in where a corner lands and are not taken.
		RectangleF quad_bounds(const Vector2F& position,
			const Vector2F& size,
			float rotation,
			const Vector2F& origin_pixels)
		{
			const Vector2F top_left = position - origin_pixels;

			// The same branch build_quad takes, for the same reason: this
			// runs on the cull path, per object per view, and almost nothing
			// is rotated. For an unrotated sprite the box is the rectangle
			// itself, and no corner needs folding.
			if (rotation == 0.0f)
			{
				return RectangleF(top_left, size);
			}

			const float cosine = std::cos(rotation);
			const float sine = std::sin(rotation);

			Point2F points[4];
			for (int i = 0; i < 4; i++)
			{
				const Vector2F offset = CORNERS[i] * size - origin_pixels;
				points[i] = Vector2F(
					position.x + offset.x * cosine - offset.y * sine,
					position.y + offset.x * sine + offset.y * cosine);
			}
			return RectangleF::bounding_box_of(points);
		}
	}

	void build_sprite_quad(const RectangleF& destination,
		const RectangleI& source,
		const Vector2F& texture_size,
		const Colour& tint,
		float rotation,
		const Vector2F& origin,
		SpriteFlip flip,
		SpriteVertex* corners)
	{
		// EACH EDGE, THEN THE DIFFERENCE - not the position and the size. A
		// destination of x=10.4 width=8 has a right edge of 18.4, and the two
		// orders disagree about it: truncating each edge gives 18 - 10 = 8,
		// where truncating the position and keeping the width gives 10 + 8 = 18
		// as well, but at x=10.9 the first gives 18 - 10 = 8 and rounding the
		// size first would give 7. The edges-first order is the one this engine
		// has always drawn, because the rectangle went through a RECT of four
		// truncated longs before anything measured it. (Pick the worked example
		// with care: x=10.9 is answered 8 by both orders and demonstrates
		// nothing.)
		const float left = std::trunc(destination.left());
		const float top = std::trunc(destination.top());
		const float right = std::trunc(destination.right());
		const float bottom = std::trunc(destination.bottom());

		build_quad(Vector2F(left, top), Vector2F(right - left, bottom - top),
			source, texture_size, tint, rotation, origin, flip, corners);
	}

	void build_scaled_quad(const Vector2F& position,
		float scale,
		const RectangleI& source,
		const Vector2F& texture_size,
		const Colour& tint,
		float rotation,
		const Vector2F& origin,
		SpriteVertex* corners)
	{
		const Vector2F size(static_cast<float>(source.width) * scale,
			static_cast<float>(source.height) * scale);

		build_quad(position, size, source, texture_size, tint, rotation,
			origin, SpriteFlip::none, corners);
	}

	void build_glyph_quad(const Vector2F& position,
		float scale,
		const Glyph& glyph,
		const Vector2F& pen,
		const Vector2F& texture_size,
		const Colour& tint,
		float rotation,
		const Vector2F& origin,
		SpriteVertex* corners)
	{
		// THE SCALED FORM, NOT THE RECTANGLE FORM. A destination rectangle
		// truncates each of its edges to a whole pixel, so a line of text laid
		// out through one would jitter against its own advance, which is
		// fractional in most fonts.
		const Vector2F glyph_origin(origin.x - pen.x,
			origin.y - (pen.y + glyph.y_offset));

		build_scaled_quad(position, scale, glyph.subrect, texture_size, tint,
			rotation, glyph_origin, corners);
	}

	RectangleF sprite_quad_bounds(const RectangleF& destination,
		const RectangleI& source,
		float rotation,
		const Vector2F& origin)
	{
		// The edges build_sprite_quad truncates, truncated the same way, so
		// the box is the drawn one and not the one asked for: a destination
		// at x=10.9 draws from x=10, and a box that started at 10.9 would be
		// a column short on its left.
		const float left = std::trunc(destination.left());
		const float top = std::trunc(destination.top());
		const float right = std::trunc(destination.right());
		const float bottom = std::trunc(destination.bottom());
		const Vector2F size(right - left, bottom - top);

		// Unscaled source texels into destination pixels, which is the
		// origin_ratio * size term of build_quad in one step, with the same
		// stand-in for a source of no width.
		const Vector2F origin_pixels(
			origin.x / without_zero(static_cast<float>(source.width)) * size.x,
			origin.y / without_zero(static_cast<float>(source.height)) * size.y);

		return quad_bounds(Vector2F(left, top), size, rotation, origin_pixels);
	}

	RectangleF text_quad_bounds(const Vector2F& position,
		float scale,
		const Vector2F& measured_size,
		float rotation,
		const Vector2F& origin)
	{
		// A string's origin is in the same unscaled texels a glyph's is, and
		// build_glyph_quad scales it with the glyph - so in pixels it is the
		// origin times the scale, whatever the string measures.
		return quad_bounds(position, measured_size * scale, rotation,
			origin * scale);
	}
}
