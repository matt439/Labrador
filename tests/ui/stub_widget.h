#pragma once

#include "engine/ui/widget.h"
#include "engine/render/colour.h"
#include "engine/math/rectanglef.h"

// A widget that is nothing but a rectangle and a colour.
//
// engine/ui's real widgets cannot be constructed without a device - UiText
// resolves a SpriteFont handle and measures with it in its constructor - but
// focus and navigation never touch a font, a texture or a sprite batch. They
// read bounds() and write set_colour(). This is that surface and no more, so
// the two mechanisms the module exists for are testable today rather than
// after the renderer seam lands.
class StubWidget final : public artattack::UiWidget
{
public:
	StubWidget(float x, float y, float width, float height) :
		UiWidget("stub"), bounds_(x, y, width, height)
	{
	}

	void update(float /*dt*/) override {}

	void draw(artattack::DrawList& /*draw_list*/) const override
	{
		// Never called by anything under test; a focus group draws nothing.
	}

	mattmath::RectangleF bounds() const override { return this->bounds_; }

	void set_colour(const artattack::Colour& colour) override
	{
		this->colour_ = colour;
	}

	const artattack::Colour& colour() const { return this->colour_; }

private:
	mattmath::RectangleF bounds_;
	artattack::Colour colour_ = artattack::Colour::black;
};
