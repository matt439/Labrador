#include <doctest/doctest.h>

#include "engine/core/name_table.h"
#include "engine/math/rectanglei.h"
#include "engine/render/animation_object.h"
#include "engine/render/animation_strip.h"
#include "engine/render/render_resources.h"
#include "engine/render/sprite_frame.h"
#include "engine/render/sprite_sheet.h"

#include <memory>
#include <stdexcept>
#include <utility>

// The frame clock, with no device and no texture behind the sheet.
//
// WHY THIS FILE EXISTS. AnimationObject was the largest .cpp under
// engine/render/ that no test named - 156 lines, and
// docs/review/backend-equivalence-2/ counted it as one of the twelve in that
// state under GAPS.md item 8. What it carries is not accessor-and-composition
// like DrawObject next door: it is arithmetic on a clock, read on the update
// path as well as the draw path, and every term of it was unstated.
//
// WHAT IS OBSERVABLE HERE AND WHAT IS NOT. frame_index_ is private with no
// accessor, and the two draw() overloads need a DrawList, which needs a view,
// which needs a device. So this file cannot read the index and does not try:
// it reads the clock through is_paused(), which is the one thing the class
// tells a caller for free, and through WHEN the latch happens - a count of
// updates that is exact rather than approximate, because every frame time and
// step below is a power of two and /fp:precise is load-bearing here. Reading
// the index back needs the null backend's recording, which is
// tests/render/null_tests.cpp and another configuration.
//
// A sheet with an unresolved TextureHandle is legal and deliberate. The handle
// is dereferenced inside SpriteSheet::draw and nowhere on the path this file
// walks, so the whole clock runs against content that has no texture at all.

namespace
{
	using labrador::AnimationObject;
	using labrador::AnimationStrip;
	using labrador::NameTable;
	using labrador::RenderResources;
	using labrador::SpriteFrame;
	using labrador::SpriteSheet;
	using labrador::TextureHandle;
	using mattmath::RectangleI;

	// THE STATE IS protected AND THAT IS THE CLASS'S SHAPE, which is the same
	// argument draw_object_tests.cpp makes about DrawObject: an AnimationObject
	// is a base for a drawable a game writes, so what it carries is for the
	// subclass. A derived probe is how that caller sees the surface, so it is
	// how this file sees it too.
	class Probe : public AnimationObject
	{
	public:
		using AnimationObject::AnimationObject;

		using AnimationObject::is_paused;
		using AnimationObject::pause;
		using AnimationObject::play;
		using AnimationObject::reset;
		using AnimationObject::set_animation_strip_and_reset;
		using AnimationObject::set_frame_index;
		using AnimationObject::set_frame_time;
		using AnimationObject::set_frame_time_to_default;
		using AnimationObject::stop;
		using AnimationObject::update;
	};

	// Two sheets, because set_animation_strip_and_reset moves both together and
	// a test that only ever has one sheet cannot say so. Neither has a texture.
	class Content
	{
	public:
		Content() : resources()
		{
			NameTable<AnimationStrip> strips("animation strip");
			strips.add("three",
				AnimationStrip(RectangleI(0, 0, 8, 8), 3, 0.25f, false));
			strips.add("three_looping",
				AnimationStrip(RectangleI(0, 0, 8, 8), 3, 0.25f, true));
			strips.add("one",
				AnimationStrip(RectangleI(0, 0, 8, 8), 1, 0.25f, false));
			this->resources.add_sprite_sheet("sheet",
				std::make_unique<SpriteSheet>(TextureHandle(),
					NameTable<SpriteFrame>("sprite frame"),
					std::move(strips)));

			NameTable<AnimationStrip> slow("animation strip");
			slow.add("two_slow",
				AnimationStrip(RectangleI(0, 0, 8, 8), 2, 1.0f, false));
			this->resources.add_sprite_sheet("other",
				std::make_unique<SpriteSheet>(TextureHandle(),
					NameTable<SpriteFrame>("sprite frame"),
					std::move(slow)));
		}

		RenderResources resources;
	};

	// How the clock is read at all: a non-looping strip's only public statement
	// is that it eventually stops, and WHEN it stops is the whole of the
	// arithmetic. The cap stops a looping strip hanging the run and is reported
	// as -1 rather than silently passing.
	int updates_until_paused(Probe& animation, float dt)
	{
		for (int tick = 1; tick <= 100; tick++)
		{
			animation.update(dt);
			if (animation.is_paused())
			{
				return tick;
			}
		}
		return -1;
	}
}

TEST_CASE("an animation takes its frame time from the strip it was built on")
{
	Content content;
	Probe fast("sheet", "one", &content.resources);
	Probe slow("other", "two_slow", &content.resources);

	// One frame at 0.25 and two frames at 1.0, stepped by their own frame time
	// each: the counts differ because the strips do, which is the only way this
	// file can say the constructor read frame_time() at all.
	CHECK(updates_until_paused(fast, 0.25f) == 2);
	CHECK(updates_until_paused(slow, 1.0f) == 3);
}

TEST_CASE("a step of exactly one frame time does not advance the frame")
{
	Content content;
	Probe on_the_boundary("sheet", "one", &content.resources);
	Probe past_it("sheet", "one", &content.resources);

	// The comparison is `time_elapsed_ > frame_time_`, strictly, so the step
	// that lands exactly on the boundary is the one that does nothing. A
	// one-frame strip makes the difference a whole update: stepping by the
	// frame time needs two, stepping past it needs one.
	//
	// Both values are exactly representable and the accumulator adds and
	// subtracts the same number, so this is an exact statement about `>` versus
	// `>=` rather than a tolerance.
	CHECK(updates_until_paused(on_the_boundary, 0.25f) == 2);
	CHECK(updates_until_paused(past_it, 0.5f) == 1);
}

TEST_CASE("a non-looping animation stops on its last frame and stays stopped")
{
	Content content;
	Probe animation("sheet", "three", &content.resources);

	CHECK_FALSE(animation.is_paused());
	CHECK(updates_until_paused(animation, 0.25f) == 4);

	// It latched rather than ran on: the index walked past the end, was put
	// back, and the object paused itself. Further updates return at the top.
	animation.update(0.25f);
	animation.update(100.0f);
	CHECK(animation.is_paused());
}

TEST_CASE("a looping animation never stops on its own")
{
	Content content;
	Probe animation("sheet", "three_looping", &content.resources);

	// Same strip length and frame time as the case above, and the only
	// difference is the flag. -1 is the helper's cap, i.e. a hundred updates
	// without pausing.
	CHECK(updates_until_paused(animation, 0.25f) == -1);
	CHECK_FALSE(animation.is_paused());
}

TEST_CASE("a long step advances one frame, it does not skip several")
{
	Content content;
	Probe animation("sheet", "three", &content.resources);

	// Ten frame times in one update, on a three-frame strip. If the advance
	// were a division it would latch immediately; it is an if rather than a
	// while, so the index moves once per update however much time arrived, and
	// the strip still takes one update per frame to walk.
	//
	// This is a statement of what the code does rather than of what it should
	// do: a client that drops a long frame and then hands the whole of it to
	// update() gets a slow animation, not a skipped one.
	CHECK(updates_until_paused(animation, 2.5f) == 3);
}

TEST_CASE("reset returns the clock but does not restart a stopped animation")
{
	Content content;
	Probe animation("sheet", "three", &content.resources);

	CHECK(updates_until_paused(animation, 0.25f) == 4);

	// reset() writes the index and the accumulator and touches the flag
	// nowhere, so a finished animation that is only reset stays finished. This
	// is the one place the four verbs are easy to get wrong from their names.
	animation.reset();
	CHECK(animation.is_paused());

	animation.play();
	CHECK_FALSE(animation.is_paused());
	CHECK(updates_until_paused(animation, 0.25f) == 4);
}

TEST_CASE("pause holds the clock where it is and play resumes from there")
{
	Content content;
	Probe animation("sheet", "three", &content.resources);

	animation.update(0.25f);
	animation.update(0.25f);
	animation.pause();
	CHECK(animation.is_paused());

	// An update while paused returns at the top, so it costs the clock nothing
	// - the two updates below are not part of the count that follows.
	animation.update(0.25f);
	animation.update(0.25f);

	animation.play();
	CHECK_FALSE(animation.is_paused());

	// Two updates in before the pause, so two frames of the three remain.
	CHECK(updates_until_paused(animation, 0.25f) == 2);
}

TEST_CASE("stop pauses and rewinds together")
{
	Content content;
	Probe animation("sheet", "three", &content.resources);

	animation.update(0.25f);
	animation.update(0.25f);
	animation.stop();
	CHECK(animation.is_paused());

	// stop() is pause() plus reset(), so what play() resumes is the beginning
	// rather than the middle - the full four updates again, where the case
	// above resumed with two left.
	animation.play();
	CHECK(updates_until_paused(animation, 0.25f) == 4);
}

TEST_CASE("a frame index outside the strip is refused, and it names the number")
{
	Content content;
	Probe animation("sheet", "three", &content.resources);

	CHECK_THROWS_AS(animation.set_frame_index(-1), std::out_of_range);
	CHECK_THROWS_AS(animation.set_frame_index(3), std::out_of_range);

	// The last frame is in range, and landing on it is not the same as
	// finishing: the strip stops on the update that tries to leave it.
	animation.set_frame_index(2);
	CHECK_FALSE(animation.is_paused());
	CHECK(updates_until_paused(animation, 0.25f) == 2);
}

TEST_CASE("a frame time set by hand overrides the strip, and can be given back")
{
	Content content;
	Probe animation("sheet", "one", &content.resources);

	// The strip says 0.25. Ask for four times that and step by the strip's
	// own number: the accumulator now needs five steps to get past the
	// boundary where it needed two, which is the factor asked for and is a
	// sharper statement than "it takes longer".
	animation.set_frame_time(1.0f);
	CHECK(updates_until_paused(animation, 0.25f) == 5);

	animation.play();
	animation.reset();
	animation.set_frame_time_to_default();
	CHECK(updates_until_paused(animation, 0.25f) == 2);
}

TEST_CASE("changing strip moves the sheet with it and takes the new frame time")
{
	Content content;
	Probe animation("sheet", "three", &content.resources);

	animation.update(0.25f);
	animation.update(0.25f);

	// The new strip lives in the other sheet, which is the pairing the header
	// insists on: a strip handle resolved against one sheet indexes nothing
	// meaningful in another, so the two move in one call or not at all.
	animation.set_animation_strip_and_reset("other", "two_slow");

	// Two frames at 1.0 rather than three at 0.25, and the clock was reset, so
	// the count is the new strip's from the beginning rather than the old
	// strip's from where it had got to.
	CHECK(updates_until_paused(animation, 1.0f) == 3);
}

TEST_CASE("a sheet or a strip that is not there is refused at construction")
{
	Content content;

	// Both throw out_of_range naming what was asked for - the sheet from the
	// resource table, the strip from the sheet's own name table - and both do
	// it in the constructor rather than at the first draw, which is the whole
	// point of resolving a name once.
	CHECK_THROWS_AS(Probe("no_such_sheet", "three", &content.resources),
		std::out_of_range);
	CHECK_THROWS_AS(Probe("sheet", "no_such_strip", &content.resources),
		std::out_of_range);
}
