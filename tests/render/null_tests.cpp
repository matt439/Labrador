#include <doctest/doctest.h>

#include "engine/render/null/recording.h"
#include "engine/render/camera.h"
#include "engine/render/colour.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"

#include <stdexcept>
#include <tuple>
#include <vector>

// Drawing, asserted with no graphics API at all.
//
// COMPILED ONLY IN THE null CONFIGURATION, which is what the backend being a
// build-time choice means: this file names engine/render/null/recording.h, and
// tests/render/CMakeLists.txt adds it to RenderTests only when that backend is
// the one being built. Asking for a backend that was not built is a missing
// symbol (T5), and that applies to a test as much as to a client.
//
// WHAT THESE COVER THAT RenderPixelTests CANNOT. That file needs a device: WARP
// on Direct3D, a real driver on OpenGL, and neither is available on a build
// machine - which is why CI runs eleven of twelve entries against the GL
// preset. These run everywhere, and they assert something the pixel tests
// cannot even see: not what a frame looked like, but which sprites were
// submitted, in what order, from which texture, into which view. A cull that
// dropped the wrong object, a drawable that emitted two quads where it meant
// one, a view that recorded into its neighbour - none of those is visible in a
// 64x64 read-back and all of them are here.
//
// AND WHAT THEY DO NOT. Nothing about colour, blending, filtering or
// rasterisation, because nothing here rasterises. The geometry is real - this
// backend runs engine/render/sprite_geometry.cpp exactly as the other two do -
// so a corner position asserted here is the corner a device would have been
// given. What happens to it afterwards is the pixel tests' business.

namespace
{
	using namespace labrador;
	using namespace mattmath;

	// A renderer, a table, and content loaded through the real load path.
	//
	// THE LOADERS ARE NOT STUBBED. load_texture_asset opens ./content/quad.dds,
	// parses it and rejects it if it is wrong; only the last step - the bytes
	// becoming a device texture - is this backend's, and it keeps the size.
	// So a test here exercises the same file reading a client does.
	class Harness
	{
	public:
		Harness()
		{
			// No window. The null backend takes one and ignores it, which is
			// the whole reason this can run on a machine with no display.
			this->renderer_.create_device(nullptr, 640, 480, 4);
			this->renderer_.set_resources(&this->resources_);

			load_texture_asset(this->renderer_, this->resources_,
				"./content/", "quad");
			this->quad = this->resources_.resolve_texture("quad");

			load_font_asset(this->renderer_, this->resources_, "./content/",
				"courier_new_bold_16");
			this->font = this->resources_.resolve_sprite_font(
				"courier_new_bold_16");
		}

		DrawList begin(int views = 1)
		{
			this->renderer_.begin_frame();
			this->renderer_.set_view_count(views);
			return this->renderer_.view(0);
		}

		DrawList view(int index) { return this->renderer_.view(index); }

		// A COPY, WHERE THE ENGINE HANDS BACK A REFERENCE. recording.h says the
		// recording is valid until the next begin_frame, which is the right
		// contract for a frame path and the wrong one for a test that wants to
		// compare two frames - the second begin_frame clears the vector the
		// first frame's reference is still pointing at, and every assertion
		// against it silently starts describing the second frame. This test
		// file had that bug for exactly one run.
		std::vector<RecordedSprite> end()
		{
			this->renderer_.submit();
			return recorded_sprites(this->renderer_);
		}

		Renderer& renderer() { return this->renderer_; }
		RenderResources& resources() { return this->resources_; }

		static RectangleI whole() { return RectangleI(0, 0, 2, 2); }

		TextureHandle quad;
		FontHandle font;

	private:
		Renderer renderer_;
		RenderResources resources_;
	};
}

TEST_CASE("a sprite is recorded with the corners a device would have been given")
{
	Harness harness;

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(10.0f, 20.0f, 8.0f, 4.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);
	const std::vector<RecordedSprite> drawn = harness.end();

	REQUIRE(drawn.size() == 1);
	CHECK(drawn[0].view == 0);
	CHECK(drawn[0].texture.index() == harness.quad.index());

	// The same arithmetic engine/render/sprite_geometry.cpp gives the real
	// backends, which is what makes the number worth asserting.
	CHECK(drawn[0].corners[0].position.x == doctest::Approx(10.0f));
	CHECK(drawn[0].corners[0].position.y == doctest::Approx(20.0f));
	CHECK(drawn[0].corners[3].position.x == doctest::Approx(18.0f));
	CHECK(drawn[0].corners[3].position.y == doctest::Approx(24.0f));
}

TEST_CASE("the camera reaches the recording, because it is applied before it")
{
	Harness harness;

	DrawList list = harness.begin();
	list.set_camera(Camera(4.0f, 4.0f, 1.0f));
	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(4.0f, 4.0f, 4.0f, 4.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);
	const std::vector<RecordedSprite> drawn = harness.end();

	// view = (world - translation) * scale, so world (4,4) is at the top left
	// of the view. A backend never sees a camera - the seam applies it as each
	// draw is recorded - and this is that statement, asserted where the pixel
	// tests can only infer it from where the ink ended up.
	REQUIRE(drawn.size() == 1);
	CHECK(drawn[0].corners[0].position.x == doctest::Approx(0.0f));
	CHECK(drawn[0].corners[0].position.y == doctest::Approx(0.0f));
}

TEST_CASE("one glyph is one sprite, and the atlas is the texture")
{
	Harness harness;

	DrawList list = harness.begin();
	list.draw_text(harness.font, L"AB", Vector2F(0.0f, 0.0f), Colour::white,
		1.0f, 0.0f, Vector2F::ZERO, 0.0f);
	const std::vector<RecordedSprite> drawn = harness.end();

	// TWO CHARACTERS, TWO QUADS - which no read-back can tell from one quad of
	// the same shape, and which is the whole of what draw_text does now that
	// the walk is the engine's.
	REQUIRE(drawn.size() == 2);
	CHECK(drawn[0].texture.index() == drawn[1].texture.index());

	// And not the quad texture: a font's atlas is a texture in the same table,
	// under a name no manifest can produce.
	CHECK(drawn[0].texture.index() != harness.quad.index());

	// The second glyph is one advance to the right of the first, on the same
	// rows - the term RenderPixelTests establishes by measuring ink, here read
	// straight off the corners.
	const float advance =
		drawn[1].corners[0].position.x - drawn[0].corners[0].position.x;
	CHECK(advance > 0.0f);
	CHECK(drawn[1].corners[0].position.y ==
		doctest::Approx(drawn[0].corners[0].position.y));

	// A space draws nothing and still steps the pen, which is a fact about the
	// walk that a picture can only show by absence.
	DrawList spaced = harness.begin();
	spaced.draw_text(harness.font, L"A B", Vector2F(0.0f, 0.0f), Colour::white,
		1.0f, 0.0f, Vector2F::ZERO, 0.0f);
	const std::vector<RecordedSprite> with_space = harness.end();

	REQUIRE(with_space.size() == 2);
	CHECK(with_space[1].corners[0].position.x ==
		doctest::Approx(drawn[1].corners[0].position.x + advance));
}

TEST_CASE("the recording is in view order, then in call order")
{
	Harness harness;

	DrawList first = harness.begin(3);
	DrawList second = harness.view(1);
	DrawList third = harness.view(2);

	// Filled out of order on purpose. Several workers draw into their own views
	// at once, so the order the views are *finished* in is not the order the
	// seam promises - and this backend is the only one that can be asked what
	// order it got.
	third.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 1.0f, 1.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);
	first.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(1.0f, 0.0f, 1.0f, 1.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);
	second.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(2.0f, 0.0f, 1.0f, 1.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);
	first.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(3.0f, 0.0f, 1.0f, 1.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);

	const std::vector<RecordedSprite> drawn = harness.end();

	REQUIRE(drawn.size() == 4);
	CHECK(drawn[0].view == 0);
	CHECK(drawn[1].view == 0);
	CHECK(drawn[2].view == 1);
	CHECK(drawn[3].view == 2);

	// Within a view, the order the calls were made in.
	CHECK(drawn[0].corners[0].position.x == doctest::Approx(1.0f));
	CHECK(drawn[1].corners[0].position.x == doctest::Approx(3.0f));
}

TEST_CASE("the seam's own rules hold here exactly as they do on a device")
{
	Harness harness;

	SUBCASE("a view nobody declared cannot be drawn into")
	{
		std::ignore = harness.begin(1);
		CHECK_THROWS_AS(std::ignore = harness.view(1), std::out_of_range);
	}

	SUBCASE("dropping a view that was drawn into is refused")
	{
		DrawList list = harness.begin(2);
		std::ignore = list;
		harness.view(1).draw_sprite(harness.quad, Harness::whole(),
			RectangleF(0.0f, 0.0f, 1.0f, 1.0f), Colour::white, 0.0f,
			Vector2F::ZERO, SpriteFlip::none, 0.0f);

		// Lowering the count past a view holding a recording strands it. On
		// Direct3D that is commands left inside a deferred context, surfacing
		// in the next frame; here it is a vector nobody reads. The seam refuses
		// it either way, and a client that gets this wrong finds out in
		// whichever configuration it runs.
		CHECK_THROWS_AS(harness.renderer().set_view_count(1),
			std::logic_error);
	}

	SUBCASE("more views than the capacity is refused")
	{
		harness.renderer().begin_frame();
		CHECK_THROWS_AS(harness.renderer().set_view_count(5),
			std::out_of_range);
	}
}

TEST_CASE("the load path is the real one, and its failures are the real ones")
{
	Harness harness;

	// The bytes are thrown away here, but everything before that is not: a
	// texture the manifest names and the disk has not is the same throw in
	// every configuration, and it is worth having somewhere a build machine can
	// run it.
	CHECK_THROWS_AS(load_texture_asset(harness.renderer(),
		harness.resources(), "./content/", "no_such_texture"),
		std::out_of_range);

	CHECK_THROWS_AS(std::ignore =
		harness.resources().resolve_texture("never_loaded"),
		std::out_of_range);
}

TEST_CASE("reading pixels back says plainly that there are none")
{
	Harness harness;

	std::ignore = harness.begin();
	harness.end();

	// NOT A BLACK RECTANGLE. A backend that answered this with zeroes would let
	// RenderPixelTests run against it and fail one assertion at a time, which
	// is a worse day than being told the configuration cannot answer.
	std::vector<unsigned char> pixels;
	CHECK_THROWS_AS(harness.renderer().read_back_buffer(pixels),
		std::logic_error);
}
