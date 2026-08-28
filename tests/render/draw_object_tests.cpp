#include <doctest/doctest.h>

#include "engine/render/draw_object.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/segment.h"
#include "engine/math/vector2f.h"

#include <cmath>

// The per-draw state every drawable carries, with no device and no table.
//
// WHY THIS FILE EXISTS AT ALL. DrawObject is accessor-and-composition code -
// six members, six pairs of virtual accessors and setters - and it had no test
// of any kind, which docs/review/backend-equivalence-2/ counted as one of ten
// .cpp under engine/render/ in that state. Most of it needs no assertion: a
// setter that stores is checked by the compiler. One of them did not store,
// and that is the reason the rest are here too - a file with one case in it
// reads as a bug report, where a file that states the whole surface reads as
// the surface.
//
// set_draw_rotation_by_rectangle_rotated was an empty body with a TODO. It
// took a RectangleRotated and discarded it, so a caller turning a sprite to
// match a collision shape got a sprite that never turned and no diagnostic of
// any kind. GAPS.md raised it; this is the assertion that keeps it answered.

namespace
{
	using labrador::Colour;
	using labrador::DrawObject;
	using labrador::SpriteFlip;
	using mattmath::Point2F;
	using mattmath::RectangleRotated;
	using mattmath::Vector2F;

	// A rectangle turned by `radians` about the origin, built the way the
	// collision module builds one: an orthonormal axis pair rather than an
	// angle, because that is what the type takes.
	RectangleRotated turned(float radians)
	{
		const float cosine = std::cos(radians);
		const float sine = std::sin(radians);
		return RectangleRotated(Point2F(0.0f, 0.0f), Point2F(cosine, sine),
			Point2F(-sine, cosine), Vector2F(4.0f, 2.0f));
	}

	// THE STATE IS protected AND THAT IS THE CLASS'S SHAPE, not an obstacle to
	// get around: DrawObject is a base, and what it carries is for the
	// drawables that derive from it - Visual, TextureObject, TextObject. A
	// derived probe is therefore how a caller sees this surface, so it is how
	// the test sees it too.
	class Probe : public DrawObject
	{
	public:
		using DrawObject::colour;
		using DrawObject::draw_rotation;
		using DrawObject::flip;
		using DrawObject::layer_depth;
		using DrawObject::origin;
		using DrawObject::render_resources;
		using DrawObject::set_colour;
		using DrawObject::set_draw_rotation;
		using DrawObject::set_draw_rotation_by_rectangle_rotated;
		using DrawObject::set_flip;
		using DrawObject::set_layer_depth;
		using DrawObject::set_origin;
	};
}

TEST_CASE("a drawable starts with the defaults its constructor names")
{
	Probe object;

	CHECK(object.colour() == Colour::white);
	CHECK(object.draw_rotation() == 0.0f);
	CHECK(object.origin() == Vector2F::ZERO);
	CHECK(object.flip() == SpriteFlip::none);
	CHECK(object.layer_depth() == 0.0f);
	CHECK(object.render_resources() == nullptr);
}

TEST_CASE("every setter stores what it was given")
{
	Probe object;

	object.set_colour(Colour(0.25f, 0.5f, 0.75f, 1.0f));
	object.set_draw_rotation(1.5f);
	object.set_origin(Vector2F(3.0f, 4.0f));
	object.set_flip(SpriteFlip::both);
	object.set_layer_depth(0.5f);

	CHECK(object.colour() == Colour(0.25f, 0.5f, 0.75f, 1.0f));
	CHECK(object.draw_rotation() == doctest::Approx(1.5f));
	CHECK(object.origin().x == doctest::Approx(3.0f));
	CHECK(object.origin().y == doctest::Approx(4.0f));
	CHECK(object.flip() == SpriteFlip::both);
	CHECK(object.layer_depth() == doctest::Approx(0.5f));
}

TEST_CASE("CONTRACT: a rotated rectangle sets the draw rotation to its own angle")
{
	Probe object;

	// A quarter turn clockwise on screen, which with y running down is a
	// positive angle - the same sign the pixel contract pins for a sprite.
	object.set_draw_rotation_by_rectangle_rotated(
		turned(3.14159265f / 2.0f));
	CHECK(object.draw_rotation() ==
		doctest::Approx(3.14159265f / 2.0f).epsilon(0.0001));

	// And back, which is the assertion that catches a setter that stores
	// nothing: a second call has to move the value it left.
	object.set_draw_rotation_by_rectangle_rotated(turned(0.0f));
	CHECK(object.draw_rotation() == doctest::Approx(0.0f));

	// A negative angle survives as one rather than as its 2*pi complement,
	// because atan2 answers in (-pi, pi] and nothing here normalises. A caller
	// comparing two rotations for equality gets the answer it wrote.
	object.set_draw_rotation_by_rectangle_rotated(turned(-0.75f));
	CHECK(object.draw_rotation() == doctest::Approx(-0.75f).epsilon(0.0001));
}

TEST_CASE("CONTRACT: it sets the rotation and leaves the origin alone")
{
	Probe object;
	object.set_origin(Vector2F(2.0f, 2.0f));

	// A rectangle centred somewhere other than the origin, to make the point
	// that its centre is not consulted: a sprite turns about ITS origin, in
	// unscaled source texels, and this class does not know the source.
	RectangleRotated rectangle = turned(0.5f);
	rectangle.set_center(Point2F(100.0f, 50.0f));

	object.set_draw_rotation_by_rectangle_rotated(rectangle);

	CHECK(object.draw_rotation() == doctest::Approx(0.5f).epsilon(0.0001));
	CHECK(object.origin().x == doctest::Approx(2.0f));
	CHECK(object.origin().y == doctest::Approx(2.0f));
}
