#include "samples/components/nine_slice.h"

#include "engine/math/vector2f.h"

#include <algorithm>
#include <stdexcept>

using namespace labrador;

namespace components
{
	namespace
	{
		// The three spans across one axis, as four edges: start, end of the
		// first border, start of the second, end.
		struct Cuts
		{
			float edge[4] = {};
		};

		Cuts destination_cuts(float start, float length, int first_border,
			int second_border, float scale)
		{
			float first = static_cast<float>(first_border) * scale;
			float second = static_cast<float>(second_border) * scale;

			// Squeezed: both corners shrink by one factor until they meet.
			const float borders = first + second;
			if (borders > length && borders > 0.0f)
			{
				const float shrink = length / borders;
				first *= shrink;
				second = length - first;
			}

			Cuts cuts;
			cuts.edge[0] = start;
			cuts.edge[1] = start + first;
			cuts.edge[2] = start + length - second;
			cuts.edge[3] = start + length;
			return cuts;
		}

		void check(const RectangleI& source, const SliceBorder& border,
			const RectangleF& destination, float scale)
		{
			if (border.left < 0 || border.top < 0 || border.right < 0 ||
				border.bottom < 0)
			{
				throw std::invalid_argument("nine_slice: a border is negative");
			}
			if (source.width < 0 || source.height < 0 ||
				destination.width < 0.0f || destination.height < 0.0f)
			{
				throw std::invalid_argument(
					"nine_slice: the source or destination has a negative size");
			}
			if (border.left + border.right > source.width ||
				border.top + border.bottom > source.height)
			{
				throw std::invalid_argument("nine_slice: the borders across "
					"an axis are wider than the source");
			}
			if (!(scale > 0.0f))
			{
				throw std::invalid_argument("nine_slice: scale must be above zero");
			}
		}
	}

	std::array<SlicePiece, 9> nine_slice(const RectangleI& source,
		const SliceBorder& border, const RectangleF& destination, float scale)
	{
		check(source, border, destination, scale);

		const int source_x[4] = { source.x, source.x + border.left,
			source.x + source.width - border.right, source.x + source.width };
		const int source_y[4] = { source.y, source.y + border.top,
			source.y + source.height - border.bottom,
			source.y + source.height };

		const Cuts across = destination_cuts(destination.x, destination.width,
			border.left, border.right, scale);
		const Cuts down = destination_cuts(destination.y, destination.height,
			border.top, border.bottom, scale);

		std::array<SlicePiece, 9> pieces;
		for (int row = 0; row < 3; ++row)
		{
			for (int column = 0; column < 3; ++column)
			{
				SlicePiece& piece =
					pieces[static_cast<size_t>(row * 3 + column)];
				piece.source = RectangleI(source_x[column], source_y[row],
					source_x[column + 1] - source_x[column],
					source_y[row + 1] - source_y[row]);
				piece.destination = RectangleF(across.edge[column],
					down.edge[row],
					across.edge[column + 1] - across.edge[column],
					down.edge[row + 1] - down.edge[row]);
			}
		}

		return pieces;
	}

	NineSlice::NineSlice(const std::string& name,
		TextureHandle texture,
		const RectangleI& source,
		const SliceBorder& border,
		const RectangleF& rectangle,
		const Colour& colour,
		float scale) :
		UiWidget(name),
		texture_(texture),
		source_(source),
		border_(border),
		rectangle_(rectangle),
		colour_(colour),
		scale_(scale),
		pieces_(nine_slice(source, border, rectangle, scale))
	{

	}

	void NineSlice::update(float /*dt*/)
	{

	}

	void NineSlice::draw(DrawList& draw_list) const
	{
		// widget.h: a leaf that draws checks hidden() itself.
		if (this->hidden())
		{
			return;
		}

		// A piece with no texels or no area is a border of zero, or one
		// squeezed away: drawing it would put nothing on screen.
		for (const SlicePiece& piece : this->pieces_)
		{
			if (piece.source.width == 0 || piece.source.height == 0 ||
				piece.destination.width <= 0.0f ||
				piece.destination.height <= 0.0f)
			{
				continue;
			}

			draw_list.draw_sprite(this->texture_, piece.source,
				piece.destination, this->colour_, 0.0f, Vector2F::zero,
				SpriteFlip::none, 0.0f);
		}
	}

	RectangleF NineSlice::bounds() const
	{
		return this->rectangle_;
	}

	void NineSlice::set_colour(const Colour& colour)
	{
		this->colour_ = colour;
	}

	void NineSlice::set_rectangle(const RectangleF& rectangle)
	{
		this->pieces_ = nine_slice(this->source_, this->border_, rectangle,
			this->scale_);
		this->rectangle_ = rectangle;
	}

	const Colour& NineSlice::colour() const
	{
		return this->colour_;
	}

	const std::array<SlicePiece, 9>& NineSlice::pieces() const
	{
		return this->pieces_;
	}
}
