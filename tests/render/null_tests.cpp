#include <doctest/doctest.h>

#include "engine/render/null/recording.h"
#include "engine/core/name_table.h"
#include "engine/render/animation_strip.h"
#include "engine/render/camera.h"
#include "engine/render/colour.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/sprite_frame.h"
#include "engine/render/sprite_sheet.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"

#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

// Drawing, asserted with no graphics API at all.
//
// COMPILED ONLY IN THE null CONFIGURATION, which is what the backend being a
// build-time choice means: this file names engine/render/null/recording.h, and
// tests/render/CMakeLists.txt adds it to RenderTests only when that backend is
// the one being built. Asking for a backend that was not built is a missing
// symbol (T5), and that applies to a test as much as to a client.
//
// WHAT THESE COVER THAT RenderPixelTests CANNOT. That file needs a device, and
// a build machine has one for Direct3D and not for the other two: CI runs
// thirteen of fourteen entries against the GL preset and against the Vulkan
// one, and all fourteen against every Direct3D preset - including x64-release,
// where the WARP fallback is compiled out, which is how the runner is known to
// offer an adapter rather than to fall back to one (.github/workflows/ci.yml).
// These run everywhere, and they assert something the pixel tests
// cannot even see: not what a frame looked like, but which sprites were
// submitted, in what order, from which texture, into which view. A cull that
// dropped the wrong object, a drawable that emitted two quads where it meant
// one, a view that recorded into its neighbour - none of those is visible in a
// 64x64 read-back and all of them are here.
//
// AND WHAT THEY DO NOT. Nothing about colour, blending or rasterisation,
// because nothing here rasterises. The geometry is real - this backend runs
// engine/render/sprite_geometry.cpp exactly as the other four do - so a corner
// position asserted here is the corner a device would have been given. What
// happens to it afterwards is the pixel tests' business.
//
// THE STATE A DRAW CARRIES IS THE EXCEPTION, AND IT IS A NARROW ONE. A
// RecordedSprite stamps the filter and the viewport in force when it was
// recorded, so this file can say which state each draw would have been made
// under - not what a sampler or a rasteriser then did with it, which is the
// pixel tests' business and unreachable here. That distinction is worth the
// sentence: asserting the stamp is asserting that set_filter and set_viewport
// reach the draw, which is all a recording can know and is more than nothing
// was checking before.

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
		// The table before the renderer, so it dies after one. It costs this
		// backend nothing - a null texture is two ints - and the order is kept
		// anyway, because render_resources.h states it as a term of the seam
		// and a harness that keeps it only where it is expensive is a harness
		// that teaches the rule wrong.
		RenderResources resources_;
		Renderer renderer_;
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

// --- the state a draw was recorded under ------------------------------------
//
// NOTHING ASSERTED EITHER OF THESE UNTIL NOW, on any backend. RecordedSprite
// has carried filter and viewport since it was written; the pixel tests cannot
// reach them, because that file creates one integral 64x64 view and never calls
// set_viewport, and TextureFilter::linear has no caller anywhere in the tree.
// So a backend that dropped either on the floor passed every configuration.
//
// THE set_filter CASE IS ALSO THE ONLY ODR-USE OF IT IN THE REPOSITORY, which
// is worth more than the assertion: until this file called it, a backend could
// omit the definition entirely and all five builds would link. Now the one
// configuration CI runs completely needs it to exist, which is the link error
// T5 asks for rather than a silence.

TEST_CASE("the filter in force is stamped on each draw, and changes mid-list")
{
	Harness harness;

	DrawList list = harness.begin();

	// Default first, asserted rather than assumed - point is what both clients
	// want everywhere and a backend that defaulted to linear would look
	// slightly soft in every frame and fail nothing.
	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);

	list.set_filter(TextureFilter::linear);
	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);

	// And back, because "it changed once" and "it tracks" are different
	// claims and only the second is the contract.
	list.set_filter(TextureFilter::point);
	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);

	const std::vector<RecordedSprite> drawn = harness.end();

	REQUIRE(drawn.size() == 3);
	CHECK(drawn[0].filter == TextureFilter::point);
	CHECK(drawn[1].filter == TextureFilter::linear);
	CHECK(drawn[2].filter == TextureFilter::point);
}

TEST_CASE("the viewport in force is stamped on each draw")
{
	Harness harness;

	DrawList list = harness.begin();

	// The default is the whole back buffer the harness asked for.
	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);

	// A pane, and a fractional one - which is the shape that made the two
	// device backends disagree, because GL has to reach whole pixels and used
	// to reach them twice with two different answers. The recording keeps the
	// float it was given; Viewport::pixel_rect is where the conversion lives
	// and tests/render/viewport_tests.cpp is where it is pinned.
	list.set_viewport(Viewport(0.0f, 240.5f, 640.0f, 239.5f));
	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);

	const std::vector<RecordedSprite> drawn = harness.end();

	REQUIRE(drawn.size() == 2);

	CHECK(drawn[0].viewport.x == doctest::Approx(0.0f));
	CHECK(drawn[0].viewport.y == doctest::Approx(0.0f));
	CHECK(drawn[0].viewport.width == doctest::Approx(640.0f));
	CHECK(drawn[0].viewport.height == doctest::Approx(480.0f));

	CHECK(drawn[1].viewport.y == doctest::Approx(240.5f));
	CHECK(drawn[1].viewport.height == doctest::Approx(239.5f));

	// The pane the second draw carries covers the rows the first one's does
	// not, all the way to the last: 240 + 240 against a back buffer of 480.
	CHECK(drawn[1].viewport.pixel_rect().y +
		drawn[1].viewport.pixel_rect().height == 480);
}

TEST_CASE("state set on one view does not leak into its neighbour")
{
	Harness harness;

	DrawList first = harness.begin(2);
	DrawList second = harness.view(1);

	first.set_filter(TextureFilter::linear);
	first.set_viewport(Viewport(0.0f, 0.0f, 320.0f, 480.0f));
	first.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);

	second.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);

	const std::vector<RecordedSprite> drawn = harness.end();

	REQUIRE(drawn.size() == 2);

	// THE PARALLELISM AXIS IS VIEWS, so this is not a tidiness assertion. Two
	// workers enter draw() on the same object at once and record into
	// different views; a filter or a viewport that crossed between them would
	// make the frame depend on which worker got there first, and be invisible
	// in every read-back that did not happen to catch the race.
	CHECK(drawn[0].view == 0);
	CHECK(drawn[0].filter == TextureFilter::linear);
	CHECK(drawn[0].viewport.width == doctest::Approx(320.0f));

	CHECK(drawn[1].view == 1);
	CHECK(drawn[1].filter == TextureFilter::point);
	CHECK(drawn[1].viewport.width == doctest::Approx(640.0f));
}

TEST_CASE("a frame that is never submitted contributes nothing to the next")
{
	Harness harness;

	// A sprite and then text, which is a texture change and therefore the
	// point at which a backend that batches has to write something down. Here
	// it is a push_back either way; on D3D11 the first draw's geometry is
	// already inside a deferred context by the time the second one is
	// recorded, and a deferred context keeps what is in it until something
	// takes the command list away. Two of the four backends make this case
	// trivially true and the third had to be taught it - which is the whole
	// reason it is a case rather than an assumption. See
	// docs/review/backend-equivalence/README.md, defect B.
	DrawList abandoned = harness.begin();
	abandoned.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);
	abandoned.draw_text(harness.font, L"AA", Vector2F(0.0f, 0.0f),
		Colour::white, 1.0f, 0.0f, Vector2F::ZERO, 0.0f);

	// And no end(), which is the case. The frame is begun, drawn into, and
	// abandoned - a client reaches it by catching an exception out of its own
	// draw walk and carrying on.

	std::ignore = harness.begin();
	const std::vector<RecordedSprite> drawn = harness.end();

	CHECK(drawn.empty());
}

TEST_CASE("CONTRACT: a resize arriving mid-frame restarts the frame")
{
	// renderer.h makes it a term of the seam that window_size_changed may
	// arrive between begin_frame and submit, and that what every view has
	// recorded is dropped when it does. tests/render/pixel_tests.cpp asserts it
	// on the four backends that rasterise, in pixels; this is the same term in
	// the vocabulary this file has - which sprites a frame submitted - and it
	// is the only configuration that can ask the question that way.
	//
	// IT IS ALSO THE BACKEND THE TERM WAS MISSING FROM. Nothing here is
	// destroyed by a resize: a recording is a vector of structs and there is no
	// buffer to rebuild, so this backend could hand every sprite back and be
	// internally consistent. gl/renderer.cpp already argues why it must not -
	// "what a client can rely on is the same sentence everywhere" - and it
	// argues harder here, because this is the configuration CI runs end to end
	// and the one a client is most likely to be tested in.
	Harness harness;

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 32.0f, 32.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);

	CHECK(harness.renderer().window_size_changed(320, 240));

	// The list the caller is holding is still a list, and this draw belongs to
	// the frame that is now running.
	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);

	const std::vector<RecordedSprite> drawn = harness.end();

	// One sprite, not two: what was recorded before the resize is gone.
	REQUIRE(drawn.size() == 1);

	// And it is the one drawn after it, at the pane the new size makes - the
	// views were reopened, not merely emptied. 8 rather than 32 says which
	// draw survived; 320 says the viewport was re-stamped.
	CHECK(drawn[0].corners[3].position.x == doctest::Approx(8.0f));
	CHECK(drawn[0].viewport.width == doctest::Approx(320.0f));
	CHECK(drawn[0].viewport.height == doctest::Approx(240.0f));
}

TEST_CASE("CONTRACT: a resize clears `touched`, so the layout may be re-run")
{
	// The other half of the same term, and the half only this backend can be
	// asked about. renderer.h says the return value means "re-run the layout",
	// and set_view_count throws std::logic_error for a count lowered past a
	// view something has already drawn into - so a shell that does exactly what
	// it was told, re-running its layout mid-frame from two views to one, has
	// to find those views untouched.
	//
	// This threw here and was silent on the other four, which inverted this
	// backend's own claim to be the strictest rather than the most permissive
	// (null/backend.h). Both halves came from one missing view->reset().
	Harness harness;

	DrawList first = harness.begin(2);
	first.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);
	harness.view(1).draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, 0.0f);

	CHECK(harness.renderer().window_size_changed(320, 240));

	harness.renderer().set_view_count(1);

	const std::vector<RecordedSprite> drawn = harness.end();
	CHECK(drawn.empty());
}

// --- what a sheet's own frame contributes to a draw -------------------------
//
// THE ONLY PLACE THE COMPOSITION IS OBSERVABLE. A sheet may author a pivot per
// frame, SpriteSheet::draw adds it to whatever origin the caller passed, and
// the sum is not readable anywhere afterwards: it is one argument to
// build_sprite_quad and then it is four corner positions. RenderPixelTests
// could infer it from ink, in the one configuration that has a device, for a
// class no sample draws; here it is read straight off the quad, on a build
// machine, in the configuration CI runs completely.
//
// tests/assets/sprite_sheet_loader_tests.cpp holds the other half - that the
// key is parsed, and that its sibling `rotated` is refused rather than
// silently obeyed. Neither file can assert the whole of the finding alone.

TEST_CASE("a frame's authored origin moves the quad, and the caller's adds")
{
	Harness harness;

	// A 2x2 source with a pivot at (1,1) - its centre, in unscaled source
	// texels, which is the seam's unit for an origin and the sheet's for this
	// key. Over an 8x4 destination that is half of each side: 4 pixels left and
	// 2 up.
	NameTable<SpriteFrame> frames("sprite frame");
	frames.add("plain", SpriteFrame(Harness::whole()));
	frames.add("centred",
		SpriteFrame(Harness::whole(), Vector2F(1.0f, 1.0f)));

	const SpriteSheet sheet(harness.quad, std::move(frames),
		NameTable<AnimationStrip>("animation strip"));

	const RectangleF destination(10.0f, 20.0f, 8.0f, 4.0f);

	DrawList list = harness.begin();

	// A frame with no pivot, drawn with no origin: the top left of the
	// destination, which is what every frame in the tree does today and what
	// this change must not have moved.
	sheet.draw(list, sheet.resolve_sprite_frame("plain"), destination);

	// The same draw, from the frame that carries one.
	sheet.draw(list, sheet.resolve_sprite_frame("centred"), destination);

	// And both origins at once. The caller's is a second shift in the same
	// units, so the quad lands one further step along - not at the caller's
	// alone, which is the reading a substitution would have given and is the
	// distinction this case exists for.
	sheet.draw(list, sheet.resolve_sprite_frame("centred"), destination,
		Colour::white, 0.0f, Vector2F(1.0f, 1.0f));

	const std::vector<RecordedSprite> drawn = harness.end();

	REQUIRE(drawn.size() == 3);

	CHECK(drawn[0].corners[0].position.x == doctest::Approx(10.0f));
	CHECK(drawn[0].corners[0].position.y == doctest::Approx(20.0f));

	CHECK(drawn[1].corners[0].position.x == doctest::Approx(6.0f));
	CHECK(drawn[1].corners[0].position.y == doctest::Approx(18.0f));

	CHECK(drawn[2].corners[0].position.x == doctest::Approx(2.0f));
	CHECK(drawn[2].corners[0].position.y == doctest::Approx(16.0f));

	// The destination itself never moved - an origin shifts where the rectangle
	// is anchored and does not resize it, so all three quads are 8x4.
	for (const RecordedSprite& sprite : drawn)
	{
		CHECK(sprite.corners[3].position.x - sprite.corners[0].position.x ==
			doctest::Approx(8.0f));
		CHECK(sprite.corners[3].position.y - sprite.corners[0].position.y ==
			doctest::Approx(4.0f));
	}
}

TEST_CASE("a strip's frame carries no authored origin, and says so by drawing")
{
	Harness harness;

	// The source-rectangle overloads take the caller's origin and nothing else,
	// because there is no frame to ask. Asserted rather than left to the
	// header: an implementation that reached for a pivot here would have to
	// invent which frame it belonged to.
	NameTable<SpriteFrame> frames("sprite frame");
	frames.add("centred",
		SpriteFrame(Harness::whole(), Vector2F(1.0f, 1.0f)));

	const SpriteSheet sheet(harness.quad, std::move(frames),
		NameTable<AnimationStrip>("animation strip"));

	const RectangleF destination(10.0f, 20.0f, 8.0f, 4.0f);

	DrawList list = harness.begin();
	sheet.draw(list, Harness::whole(), destination);
	const std::vector<RecordedSprite> drawn = harness.end();

	REQUIRE(drawn.size() == 1);
	CHECK(drawn[0].corners[0].position.x == doctest::Approx(10.0f));
	CHECK(drawn[0].corners[0].position.y == doctest::Approx(20.0f));
}
