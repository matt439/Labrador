#pragma once

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/render/colour.h"
#include "engine/render/renderer.h"
#include "engine/ui/widget.h"

#include <array>
#include <string>

namespace components
{
	// How many texels of the source each edge keeps at its own size: the
	// corners are these widths and heights, the edges stretch one way and the
	// centre both.
	struct SliceBorder
	{
		int left = 0;
		int top = 0;
		int right = 0;
		int bottom = 0;
	};

	// One of the nine draws: which texels, and where they land.
	struct SlicePiece
	{
		labrador::RectangleI source;
		labrador::RectangleF destination;
	};

	// The nine pieces that stretch `source` over `destination` without
	// stretching its corners, in reading order: the top row left to right,
	// then the middle row, then the bottom.
	//
	// A corner is drawn `scale` world units per texel. When the destination is
	// too narrow or too short for two corners at that scale, both shrink by
	// the same factor until they meet, and the edges and the centre between
	// them are left with no width or height - a frame squeezed past its border
	// is all corner, never inside out.
	//
	// Throws std::invalid_argument if a border is negative, the two borders
	// across an axis do not fit inside the source, the source or destination
	// has a negative size, or `scale` is not above zero.
	std::array<SlicePiece, 9> nine_slice(const labrador::RectangleI& source,
		const SliceBorder& border, const labrador::RectangleF& destination,
		float scale = 1.0f);

	// A panel, a button background or a speech bubble drawn from one small
	// image at any size: a leaf widget that draws nine_slice's pieces.
	//
	// COPY IT, DO NOT LINK IT. The README beside this file says why
	// components are copied rather than shipped as engine code.
	//
	// The source must be laid out for it: corners that look right at their
	// own size, and edges and a centre that look right stretched, which a
	// gradient across an edge does not. Point filtering keeps a pixel-art
	// border crisp; linear filtering blurs where a stretched piece meets a
	// corner, because the sampler reads across the seam.
	class NineSlice final : public labrador::UiWidget
	{
	public:
		NineSlice(const std::string& name,
			labrador::TextureHandle texture,
			const labrador::RectangleI& source,
			const SliceBorder& border,
			const labrador::RectangleF& rectangle,
			const labrador::Colour& colour = labrador::Colour::white,
			float scale = 1.0f);

		void update(float dt) override;
		void draw(labrador::DrawList& draw_list) const override;
		labrador::RectangleF bounds() const override;
		void set_colour(const labrador::Colour& colour) override;

		// Throws as nine_slice does, and leaves the panel as it was.
		void set_rectangle(const labrador::RectangleF& rectangle);

		const labrador::Colour& colour() const;
		const std::array<SlicePiece, 9>& pieces() const;

	private:
		labrador::TextureHandle texture_;
		labrador::RectangleI source_;
		SliceBorder border_;
		labrador::RectangleF rectangle_;
		labrador::Colour colour_;
		float scale_;
		std::array<SlicePiece, 9> pieces_;
	};
}
