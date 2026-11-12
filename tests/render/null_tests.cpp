#include <doctest/doctest.h>

#include "engine/core/name_table.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/animation_strip.h"
#include "engine/render/camera.h"
#include "engine/render/colour.h"
#include "engine/render/null/recording.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/sprite_frame.h"
#include "engine/render/sprite_sheet.h"
#include "engine/render/viewport.h"
#include "tests/render/null_test_harness.h"

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
// WHAT THESE COVER THAT RenderPixelTests CANNOT. That file needs a device; a
// null configuration does not build it at all. Which entries each CI
// configuration registers is tests/ctest_inventory.json, and which device
// each one draws on is .github/workflows/ci.yml.
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

using namespace labrador;
using namespace render_tests::null_backend;

TEST_CASE("the null renderer reports that it selected no device")
{
	Renderer renderer;
	renderer.create_device(nullptr, 640, 480, 1);

	const RenderDeviceInfo& info = renderer.device_info();
	CHECK(info.backend == "null");
	CHECK(info.api == "none");
	CHECK(info.device_name == "No render device");
	CHECK(info.vendor_id == 0);
	CHECK(info.device_id == 0);
	CHECK(info.kind == RenderDeviceKind::null_device);
	CHECK(info.present_mode == "none");
	CHECK_FALSE(info.requested_swap_interval.has_value());
	CHECK_FALSE(info.reported_swap_interval.has_value());
}

TEST_CASE("the null renderer records a vsync request and presents nothing differently")
{
	Renderer renderer;
	renderer.set_vsync(false);
	renderer.create_device(nullptr, 640, 480, 1);

	CHECK_FALSE(renderer.vsync());

	renderer.begin_frame();
	renderer.set_view_count(1);
	renderer.submit();
	renderer.end_frame();

	const RenderDeviceInfo& info = renderer.device_info();
	CHECK(info.present_mode == "none");
	CHECK_FALSE(info.requested_swap_interval.has_value());

	renderer.set_vsync(true);
	CHECK(renderer.vsync());
}

TEST_CASE("a sprite is recorded with the corners a device would have been given")
{
	Harness harness;

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(10.0f, 20.0f, 8.0f, 4.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
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
		Vector2F::zero, SpriteFlip::none, 0.0f);
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
		1.0f, 0.0f, Vector2F::zero, 0.0f);
	const std::vector<RecordedSprite> drawn = harness.end();

	// TWO CHARACTERS, TWO QUADS - which no read-back can tell from one quad of
	// the same shape, and which is the whole of what draw_text does: the glyph
	// walk is the engine's.
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
		1.0f, 0.0f, Vector2F::zero, 0.0f);
	const std::vector<RecordedSprite> with_space = harness.end();

	// One cell further than the B of "AB", and a cell is what measure_text
	// says a second character adds - not the distance between the two quads
	// above, which is a cell plus the difference between A's bearing and B's,
	// and is a cell only in a font where those agree.
	const float cell =
		harness.resources().measure_text(harness.font, L"AA").x -
		harness.resources().measure_text(harness.font, L"A").x;
	// And the cell is the atlas's, not only measure_text's: every glyph in
	// this one steps the pen 13 pixels, so a measure and a pen walk that
	// drifted together would still be caught here.
	CHECK(cell == doctest::Approx(13.0f));
	REQUIRE(with_space.size() == 2);
	CHECK(with_space[1].corners[0].position.x ==
		doctest::Approx(drawn[1].corners[0].position.x + cell));
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
		Vector2F::zero, SpriteFlip::none, 0.0f);
	first.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(1.0f, 0.0f, 1.0f, 1.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	second.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(2.0f, 0.0f, 1.0f, 1.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	first.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(3.0f, 0.0f, 1.0f, 1.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

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
			Vector2F::zero, SpriteFlip::none, 0.0f);

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
// THESE TWO ARE NOT THE ONLY COVERAGE OF FILTER AND VIEWPORT, and reading
// them as such is the mistake worth heading off. view_pixel_tests.cpp has an
// entire viewport section and sprite_pixel_tests.cpp a case that changes the
// filter mid-list on a rasteriser, so both terms reach a device there.
//
// What these two hold that it cannot is the stamp itself: what a run was
// recorded UNDER, rather than what came out of it.

TEST_CASE("the filter in force is stamped on each draw, and changes mid-list")
{
	Harness harness;

	DrawList list = harness.begin();

	// Default first, asserted rather than assumed - point is what every sample
	// and ColourWars want everywhere and a backend that defaulted to linear would look
	// slightly soft in every frame and fail nothing.
	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

	list.set_filter(TextureFilter::linear);
	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

	// And back, because "it changed once" and "it tracks" are different
	// claims and only the second is the contract.
	list.set_filter(TextureFilter::point);
	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

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
		Vector2F::zero, SpriteFlip::none, 0.0f);

	// A pane, and a fractional one - which is the shape that makes two device
	// backends disagree if either reaches whole pixels twice with two different
	// answers, as GL has to reach them at all. The recording keeps the float it
	// was given; Viewport::pixel_rect is where the conversion lives and
	// tests/render/viewport_tests.cpp is where it is pinned.
	list.set_viewport(Viewport(0.0f, 240.5f, 640.0f, 239.5f));
	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

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
		Vector2F::zero, SpriteFlip::none, 0.0f);

	second.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

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

TEST_CASE("a fresh list reads the default camera and the whole back buffer")
{
	Harness harness;

	const DrawList list = harness.begin();

	// What a list starts with, which renderer.h states and every backend has
	// to answer the same: the identity camera, and a pane that is the whole
	// buffer the harness asked create_device for.
	CHECK(list.camera() == Camera::default_camera);
	CHECK(list.viewport() == Viewport(0.0f, 0.0f, 640.0f, 480.0f));

	std::ignore = harness.end();
}

TEST_CASE("the readers return what was last set, mid-list too, and the draws agree")
{
	Harness harness;

	DrawList list = harness.begin();

	const Camera zoomed(4.0f, 4.0f, 2.0f);
	const Viewport pane(0.0f, 240.5f, 640.0f, 239.5f);
	list.set_camera(zoomed);
	list.set_viewport(pane);

	// As given: the fraction is kept, because the reader answers in the space
	// set_viewport takes and not in the whole pixels a backend snaps it to.
	CHECK(list.camera() == zoomed);
	CHECK(list.viewport() == pane);

	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(4.0f, 4.0f, 4.0f, 4.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

	// And changed mid-list, which is ordinary: the world, then the HUD.
	const Viewport second_pane(0.0f, 0.0f, 320.0f, 240.0f);
	list.set_camera(Camera::default_camera);
	list.set_viewport(second_pane);
	CHECK(list.camera() == Camera::default_camera);
	CHECK(list.viewport() == second_pane);

	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(4.0f, 4.0f, 4.0f, 4.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

	const std::vector<RecordedSprite> drawn = harness.end();

	// What the readers said is what each draw was made under: the stamped
	// viewport, and the camera through where the corners landed - world (4,4)
	// at the zoomed camera's origin and twice the size, then at (4,4) and
	// its own size under the identity.
	REQUIRE(drawn.size() == 2);
	CHECK(drawn[0].viewport == pane);
	CHECK(drawn[0].corners[0].position.x == doctest::Approx(0.0f));
	CHECK(drawn[0].corners[3].position.x == doctest::Approx(8.0f));
	CHECK(drawn[1].viewport == second_pane);
	CHECK(drawn[1].corners[0].position.x == doctest::Approx(4.0f));
	CHECK(drawn[1].corners[3].position.x == doctest::Approx(8.0f));
}

TEST_CASE("what one view's readers return does not depend on its neighbour")
{
	Harness harness;

	DrawList first = harness.begin(2);
	const DrawList second = harness.view(1);

	first.set_camera(Camera(10.0f, 20.0f, 3.0f));
	first.set_viewport(Viewport(0.0f, 0.0f, 320.0f, 480.0f));

	// The parallelism axis is views, so a drawable reading its view's camera
	// from one worker must not see what another worker set on another view.
	CHECK(first.camera() == Camera(10.0f, 20.0f, 3.0f));
	CHECK(first.viewport() == Viewport(0.0f, 0.0f, 320.0f, 480.0f));
	CHECK(second.camera() == Camera::default_camera);
	CHECK(second.viewport() == Viewport(0.0f, 0.0f, 640.0f, 480.0f));

	std::ignore = harness.end();
}

TEST_CASE("begin_frame resets what the readers return")
{
	Harness harness;

	DrawList list = harness.begin();
	list.set_camera(Camera(10.0f, 20.0f, 3.0f));
	list.set_viewport(Viewport(0.0f, 0.0f, 320.0f, 240.0f));
	std::ignore = harness.end();

	// Reset at the top of every frame, not carried across one.
	const DrawList next = harness.begin();
	CHECK(next.camera() == Camera::default_camera);
	CHECK(next.viewport() == Viewport(0.0f, 0.0f, 640.0f, 480.0f));
	std::ignore = harness.end();
}

TEST_CASE("a resize mid-frame resets what a held list's readers return")
{
	// The shape of "CONTRACT: a resize arriving mid-frame restarts the frame",
	// asked through the readers: the list the caller still holds is a list,
	// and what it reads is the restarted frame's state at the new size.
	Harness harness;

	DrawList list = harness.begin();
	list.set_camera(Camera(10.0f, 20.0f, 3.0f));
	list.set_viewport(Viewport(0.0f, 0.0f, 100.0f, 100.0f));

	CHECK(harness.renderer().window_size_changed(320, 240));

	REQUIRE(list.valid());
	CHECK(list.camera() == Camera::default_camera);
	CHECK(list.viewport() == Viewport(0.0f, 0.0f, 320.0f, 240.0f));

	std::ignore = harness.end();
}

TEST_CASE("a frame that is never submitted contributes nothing to the next")
{
	Harness harness;

	// A sprite and then text, which is a texture change and therefore the
	// point at which a backend that batches has to write something down. Here
	// it is a push_back either way; on D3D11 the first draw's geometry is
	// already inside a deferred context by the time the second one is
	// recorded, and a deferred context keeps what is in it until something
	// takes the command list away. Three of the five backends make this case
	// trivially true - gl, null and vulkan, each of which holds a frame in
	// vectors - and the two that record into a command list had to be taught
	// it, which is the whole reason it is a case rather than an assumption.
	// See docs/review/backend-equivalence/README.md, defect B.
	DrawList abandoned = harness.begin();
	abandoned.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	abandoned.draw_text(harness.font, L"AA", Vector2F(0.0f, 0.0f),
		Colour::white, 1.0f, 0.0f, Vector2F::zero, 0.0f);

	// And no end(), which is the case. The frame is begun, drawn into, and
	// abandoned - a client reaches it by catching an exception out of its own
	// draw walk and carrying on.

	std::ignore = harness.begin();
	const std::vector<RecordedSprite> drawn = harness.end();

	CHECK(drawn.empty());
}

TEST_CASE("CONTRACT: a frame submitted and never presented contributes nothing to the next")
{
	// THIS NAMES A TERM RATHER THAN ADDING COVERAGE: the sentence at
	// renderer.h's begin_frame, which every capture leans on - begin, draw,
	// submit, read, and no end_frame. Here a recording that survived into
	// the next frame is directly visible, with no rasteriser to hide it under
	// a colour that happens to come out the same.
	Harness harness;

	DrawList captured = harness.begin();
	captured.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	captured.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(8.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	REQUIRE(harness.end().size() == 2);

	// And no end_frame, which is the case.

	DrawList next = harness.begin();
	next.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(32.0f, 32.0f, 4.0f, 4.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	const std::vector<RecordedSprite> drawn = harness.end();

	REQUIRE(drawn.size() == 1);
	CHECK(drawn[0].corners[0].position.x == doctest::Approx(32.0f));
	CHECK(drawn[0].corners[0].position.y == doctest::Approx(32.0f));
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
		Vector2F::zero, SpriteFlip::none, 0.0f);

	CHECK(harness.renderer().window_size_changed(320, 240));

	// The list the caller is holding is still a list, and this draw belongs to
	// the frame that is now running.
	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

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
		Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.view(1).draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

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

TEST_CASE("submit is once per frame, and a second one adds nothing")
{
	Harness harness;

	// TEST-GAP.md's C3, and the decision it asked for is now on the seam:
	// renderer.h says a second submit adds nothing, and every backend keeps a
	// flag begin_frame clears. This is the one configuration that can assert
	// it, because "adds nothing" is a statement about a recording and the four
	// that rasterise can only be asked what a picture looked like.
	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

	harness.renderer().submit();
	const std::vector<RecordedSprite> once = recorded_sprites(harness.renderer());
	REQUIRE(once.size() == 1);

	harness.renderer().submit();
	const std::vector<RecordedSprite> twice =
		recorded_sprites(harness.renderer());

	// Not "the same size" - the same recording. A backend that appended would
	// answer two, and one that re-gathered would answer one having done the
	// work twice; the flag means neither happens.
	REQUIRE(twice.size() == 1);
	CHECK(twice[0].corners[0].position.x ==
		doctest::Approx(once[0].corners[0].position.x));
	CHECK(twice[0].corners[0].position.y ==
		doctest::Approx(once[0].corners[0].position.y));

	// And the next frame still submits, which is what the flag being cleared by
	// begin_frame means.
	DrawList second = harness.begin();
	second.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(4.0f, 4.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	const std::vector<RecordedSprite> next = harness.end();

	REQUIRE(next.size() == 1);
	CHECK(next[0].corners[0].position.x == doctest::Approx(4.0f));
}
