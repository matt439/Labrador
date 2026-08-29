#include <doctest/doctest.h>

#include "engine/ui/focus.h"
#include "engine/ui/widget.h"
#include "tests/ui/stub_widget.h"

#include <memory>
#include <string>

using labrador::Activation;
using labrador::Colour;
using labrador::FocusGroup;
using labrador::FocusStyle;
using labrador::UiContainer;
using labrador::UiObject;
using labrador::UiTexture;
using labrador::UiWidget;

namespace
{
	// A UiObject that is drawn and has no colour, which is the case
	// UiObject::set_colour's no-op exists for. Nothing in either client is one
	// today; the point is that a container holding one still works.
	class ColourlessObject final : public UiObject
	{
	public:
		ColourlessObject() : UiObject("colourless") {}

		void update(float /*dt*/) override {}
		void draw(labrador::DrawList& /*draw_list*/) const override {}
		mattmath::RectangleF bounds() const override
		{
			return mattmath::RectangleF(0.0f, 0.0f, 10.0f, 10.0f);
		}
	};

	// The exhibit, in the smallest form that proves it compiles: a row that is
	// a label and a value side by side, which is a container the cursor can
	// land on. Before this it could not exist - UiContainer was a UiObject, so
	// FocusGroup::add would not take one.
	class OptionRow final : public UiContainer
	{
	public:
		OptionRow(const std::string& name, float y) :
			UiContainer(name),
			label_(0.0f, y, 100.0f, 20.0f),
			value_(120.0f, y, 60.0f, 20.0f)
		{
			this->add_child(&this->label_);
			this->add_child(&this->value_);
		}

		const StubWidget& label() const { return this->label_; }
		const StubWidget& value() const { return this->value_; }

	private:
		StubWidget label_;
		StubWidget value_;
	};

	// A leaf with one extra behaviour, which is what "every leaf is final"
	// made impossible: the client's own version composes a UiTextDropShadow
	// and forwards four virtuals to it to add one.
	class CountingTexture final : public UiTexture
	{
	public:
		void update(float dt) override
		{
			this->UiTexture::update(dt);
			this->updates_++;
		}
		int updates() const { return this->updates_; }

	private:
		int updates_ = 0;
	};

	FocusStyle test_style()
	{
		FocusStyle style;
		style.focused = Colour::red;
		style.unfocused = Colour::blue;
		return style;
	}
}

TEST_CASE("a container draws, measures and updates its children")
{
	StubWidget a(0.0f, 0.0f, 100.0f, 50.0f);
	StubWidget b(0.0f, 100.0f, 100.0f, 50.0f);

	UiContainer container("rows");
	container.add_child(&a);
	container.add_child(&b);

	CHECK(container.child_count() == 2);

	// The union of the children's boxes, which is what the navigation walk and
	// the culler read.
	const mattmath::RectangleF bounds = container.bounds();
	CHECK(bounds.x == doctest::Approx(0.0f));
	CHECK(bounds.y == doctest::Approx(0.0f));
	CHECK(bounds.width == doctest::Approx(100.0f));
	CHECK(bounds.height == doctest::Approx(150.0f));

	container.remove_child(&b);
	CHECK(container.child_count() == 1);
	CHECK(container.bounds().height == doctest::Approx(50.0f));
}

TEST_CASE("an empty container occupies nothing, and an empty child drags nothing")
{
	UiContainer empty("empty");
	CHECK(empty.bounds().width == doctest::Approx(0.0f));
	CHECK(empty.bounds().height == doctest::Approx(0.0f));

	// RectangleF::ZERO is a point at the world origin, so unioning a nested
	// empty container drags its parent's box back to (0,0). Culling and the
	// navigation walk both read this.
	StubWidget label(900.0f, 400.0f, 100.0f, 50.0f);
	UiContainer parent("parent");
	parent.add_child(&label);
	parent.add_child(&empty);

	CHECK(parent.bounds().x == doctest::Approx(900.0f));
	CHECK(parent.bounds().y == doctest::Approx(400.0f));
}

TEST_CASE("set_colour reaches every child, through nesting and past what has none")
{
	StubWidget a(0.0f, 0.0f, 100.0f, 50.0f);
	StubWidget nested_child(0.0f, 100.0f, 100.0f, 50.0f);
	ColourlessObject colourless;

	UiContainer inner("inner");
	inner.add_child(&nested_child);

	UiContainer outer("outer");
	outer.add_child(&a);
	outer.add_child(&inner);
	// A child with no colour is an answer, not a case to detect: no cast, no
	// skip list, nothing on the path.
	outer.add_child(&colourless);

	outer.set_colour(Colour::green);

	CHECK(a.colour() == Colour::green);
	CHECK(nested_child.colour() == Colour::green);
}

TEST_CASE("a focus group can hold a compound row and paints all of it")
{
	// The case the report was filed for. UiContainer was a UiObject, so this
	// add() would not compile at all, and there was no way to write a row with
	// a value in it that the cursor could land on.
	OptionRow resolution("resolution", 0.0f);
	OptionRow full_screen("full_screen", 100.0f);

	FocusGroup group(1, test_style());
	group.add(&resolution);
	group.add(&full_screen);

	// Both halves of the focused row are painted, and both halves of the other
	// one are not. Composition is what makes this work: the group calls
	// set_colour on the row, the row calls it on its parts.
	CHECK(resolution.label().colour() == Colour::red);
	CHECK(resolution.value().colour() == Colour::red);
	CHECK(full_screen.label().colour() == Colour::blue);
	CHECK(full_screen.value().colour() == Colour::blue);

	// And it navigates by its own bounds, which are its children's union.
	CHECK(group.move(0, labrador::Direction::down));
	CHECK(group.focused(0) == &full_screen);
	CHECK(full_screen.value().colour() == Colour::red);
	CHECK(resolution.label().colour() == Colour::blue);
}

TEST_CASE("a compound row answers a press like any other entry")
{
	OptionRow row("row", 0.0f);

	int presses = 0;
	FocusGroup group(1, test_style());
	group.add(&row, [&presses] { presses++; });

	CHECK(group.activate(0) == Activation::ran);
	CHECK(presses == 1);

	// Including the third state, which is what a row-with-a-value most wants:
	// a setting that cannot be changed yet is a row, not a missing row.
	group.set_enabled(&row, false);
	CHECK(group.activate(0) == Activation::refused);
	CHECK(presses == 1);
}

TEST_CASE("a leaf can be derived from and still be held as a widget")
{
	// This test is the compile: CountingTexture existing at all is what
	// dropping `final` bought, and the client's exhibit - a line of text plus
	// a rule for building it - is this shape with a different base.
	CountingTexture texture;
	texture.set_size(mattmath::Vector2F(40.0f, 20.0f));

	UiWidget* as_widget = &texture;
	as_widget->update(0.016f);
	as_widget->update(0.016f);

	// Through the base pointer, which is how a container and a focus group
	// both hold it.
	CHECK(texture.updates() == 2);
	CHECK(as_widget->bounds().width == doctest::Approx(40.0f));

	std::unique_ptr<UiWidget> owned = std::make_unique<CountingTexture>();
	owned->set_colour(Colour::green);
	// Destroyed through UiWidget*, which is safe because the destructor is
	// virtual from GameObject down. That is the part `final` was not
	// protecting anything from.
	owned.reset();
}
