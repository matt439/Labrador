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
// THE NULL BACKEND LETS THE REAL ONES IN, AND ONLY THERE. It supplies a
// Renderer and a RenderResources with no device at all (engine/render/null/),
// so a UiText can be built and its drawing asserted rather than assumed - which
// text_widget_tests.cpp beside this one does, added to UiTests under that
// preset alone, the way tests/render/null_tests.cpp is. These tests stay on the
// stub because what they cover - focus and navigation - is arithmetic that has
// no business depending on which backend was selected.
class StubWidget final : public labrador::UiWidget
{
public:
	StubWidget(float x, float y, float width, float height) :
		UiWidget("stub"), bounds_(x, y, width, height)
	{
	}

	void update(float /*dt*/) override {}

	void draw(labrador::DrawList& /*draw_list*/) const override
	{
		// Never called by anything under test; a focus group draws nothing.
	}

	mattmath::RectangleF bounds() const override { return this->bounds_; }

	void set_colour(const labrador::Colour& colour) override
	{
		this->colour_ = colour;
	}

	const labrador::Colour& colour() const { return this->colour_; }

private:
	mattmath::RectangleF bounds_;
	labrador::Colour colour_ = labrador::Colour::black;
};
