#pragma once

#include "engine/ui/widget.h"
#include "engine/render/colour.h"
#include "engine/math/rectanglef.h"

// A widget that is nothing but a rectangle and a colour.
//
// engine/ui's real widgets cannot be constructed without a renderer - UiText
// resolves a font handle and measures with it in its constructor - but focus
// and navigation never touch a font, a texture or a batch. They read bounds()
// and write set_colour(). This is that surface and no more.
//
// THE NULL BACKEND WOULD NOW LET THE REAL ONES IN. It supplies a Renderer and
// a RenderResources with no device at all (engine/render/null/), so a UiText
// could be built here and its drawing asserted rather than assumed. That is
// worth doing and is not done: it would make these tests build only in the
// null configuration, and what they cover - focus and navigation - is
// arithmetic that has no business depending on which backend was selected. The
// right shape is a second file beside this one, added to UiTests under that
// preset alone, the way tests/render/null_tests.cpp is.
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
