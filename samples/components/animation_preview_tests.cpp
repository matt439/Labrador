#include "samples/components/animation_preview.h"
#include "samples/components/animation_preview_test_content.h"

#include <doctest/doctest.h>

#include <limits>
#include <stdexcept>

using components::AnimationPreview;
using components::animation_tests::add_sheet;
using labrador::RenderResources;

TEST_CASE("an animation changes frame at exact duration boundaries")
{
	RenderResources resources;
	add_sheet(resources, "loop");
	AnimationPreview preview(resources, resources.resolve_sheet_strip("loop", "preview"));
	CHECK(preview.frame_index() == 0);
	CHECK(preview.elapsed() == 0.0);
	preview.update(0.0f);
	CHECK(preview.elapsed() == 0.0);
	preview.update(0.125f);
	CHECK(preview.frame_index() == 0);
	preview.update(0.125f);
	CHECK(preview.frame_index() == 1);
	preview.update(0.5f);
	CHECK(preview.frame_index() == 3);
	CHECK(preview.elapsed() == 0.75);
	CHECK_FALSE(preview.finished());
	preview.update(0.25f);
	CHECK(preview.frame_index() == 0);
	CHECK(preview.elapsed() == 0.0);
}

TEST_CASE("a loop crosses multiple frames and whole cycles in one update")
{
	RenderResources resources;
	add_sheet(resources, "loop");
	AnimationPreview preview(resources, resources.resolve_sheet_strip("loop", "preview"));
	preview.update(2.875f);
	CHECK(preview.frame_index() == 3);
	CHECK(preview.elapsed() == 0.875);
	preview.update(0.375f);
	CHECK(preview.frame_index() == 1);
	CHECK(preview.elapsed() == 0.25);
	CHECK_FALSE(preview.finished());
}

TEST_CASE("a huge loop step preserves the phase without frame stepping")
{
	RenderResources resources;
	add_sheet(resources, "loop");
	AnimationPreview preview(resources, resources.resolve_sheet_strip("loop", "preview"));
	preview.update(0.375f);
	// The largest finite float is an integer multiple of this one-second cycle.
	preview.update(std::numeric_limits<float>::max());
	CHECK(preview.elapsed() == 0.375);
	CHECK(preview.frame_index() == 1);
	CHECK_FALSE(preview.finished());
}

TEST_CASE("a one-shot finishes only after the last frame duration and holds it")
{
	RenderResources resources;
	add_sheet(resources, "once", false);
	AnimationPreview preview(resources, resources.resolve_sheet_strip("once", "preview"));
	preview.update(0.75f);
	CHECK(preview.frame_index() == 3);
	CHECK_FALSE(preview.finished());
	preview.update(0.125f);
	CHECK_FALSE(preview.finished());
	preview.update(0.125f);
	CHECK(preview.finished());
	CHECK(preview.frame_index() == 3);
	CHECK(preview.elapsed() == 1.0);
	preview.update(std::numeric_limits<float>::max());
	preview.update(0.0f);
	CHECK(preview.frame_index() == 3);
	CHECK(preview.elapsed() == 1.0);
}

TEST_CASE("a one-shot clamps a multi-frame or huge jump to its last frame")
{
	RenderResources resources;
	add_sheet(resources, "once", false);
	AnimationPreview preview(resources, resources.resolve_sheet_strip("once", "preview"));
	preview.update(std::numeric_limits<float>::max());
	CHECK(preview.frame_index() == 3);
	CHECK(preview.elapsed() == 1.0);
	CHECK(preview.finished());
}

TEST_CASE("restart resets a running loop and a completed one-shot independently")
{
	RenderResources resources;
	add_sheet(resources, "loop");
	add_sheet(resources, "once", false);
	AnimationPreview loop(resources, resources.resolve_sheet_strip("loop", "preview"));
	AnimationPreview once(resources, resources.resolve_sheet_strip("once", "preview"));
	loop.update(0.625f);
	once.update(2.0f);
	loop.restart();
	CHECK(loop.elapsed() == 0.0);
	CHECK(loop.frame_index() == 0);
	CHECK_FALSE(loop.finished());
	CHECK(once.finished());
	once.restart();
	CHECK(once.elapsed() == 0.0);
	CHECK(once.frame_index() == 0);
	CHECK_FALSE(once.finished());
	once.update(0.25f);
	CHECK(once.frame_index() == 1);
	CHECK(loop.frame_index() == 0);
}

TEST_CASE("invalid time is refused before mutation even after one-shot completion")
{
	RenderResources resources;
	add_sheet(resources, "once", false);
	AnimationPreview preview(resources, resources.resolve_sheet_strip("once", "preview"));
	const float invalid[] = { -0.25f, std::numeric_limits<float>::infinity(),
		-std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() };
	const bool states[] = { false, true };
	for (const bool finished : states)
	{
		preview.restart();
		preview.update(finished ? 1.0f : 0.375f);
		const double elapsed = preview.elapsed();
		const int frame = preview.frame_index();
		for (const float dt : invalid)
		{
			CHECK_THROWS_AS(preview.update(dt), std::invalid_argument);
			CHECK(preview.elapsed() == elapsed);
			CHECK(preview.frame_index() == frame);
			CHECK(preview.finished() == finished);
		}
	}
}

TEST_CASE("an empty strip and invalid frame durations are refused at construction")
{
	RenderResources resources;
	add_sheet(resources, "empty", true, 0);
	CHECK_THROWS_AS(AnimationPreview(resources,
		resources.resolve_sheet_strip("empty", "preview")), std::invalid_argument);
	const float invalid[] = { 0.0f, -0.25f, std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::quiet_NaN() };
	for (const float frame_time : invalid)
	{
		add_sheet(resources, "invalid", true, 4, frame_time);
		CHECK_THROWS_AS(AnimationPreview(resources,
			resources.resolve_sheet_strip("invalid", "preview")), std::invalid_argument);
	}
	CHECK_THROWS_AS(AnimationPreview(resources, RenderResources::SheetStrip()),
		std::out_of_range);
}

TEST_CASE("a one-frame strip is a full-duration loop or one-shot")
{
	RenderResources resources;
	add_sheet(resources, "loop", true, 1);
	add_sheet(resources, "once", false, 1);
	AnimationPreview loop(resources, resources.resolve_sheet_strip("loop", "preview"));
	AnimationPreview once(resources, resources.resolve_sheet_strip("once", "preview"));
	loop.update(0.75f);
	once.update(0.25f);
	CHECK(loop.frame_index() == 0);
	CHECK(loop.elapsed() == 0.0);
	CHECK_FALSE(loop.finished());
	CHECK(once.frame_index() == 0);
	CHECK(once.finished());
}

TEST_CASE("small positive frame durations still keep huge steps bounded")
{
	RenderResources resources;
	add_sheet(resources, "tiny", true, 4, std::numeric_limits<float>::min());
	AnimationPreview preview(resources, resources.resolve_sheet_strip("tiny", "preview"));
	preview.update(std::numeric_limits<float>::max());
	CHECK(preview.elapsed() >= 0.0);
	CHECK(preview.elapsed() < 4.0 * static_cast<double>(std::numeric_limits<float>::min()));
	CHECK(preview.frame_index() >= 0);
	CHECK(preview.frame_index() < 4);
}

TEST_CASE("independent players of the same paired strip keep separate clocks")
{
	RenderResources resources;
	add_sheet(resources, "loop");
	const RenderResources::SheetStrip strip = resources.resolve_sheet_strip("loop", "preview");
	AnimationPreview first(resources, strip);
	AnimationPreview second(resources, strip);
	first.update(0.5f);
	CHECK(first.frame_index() == 2);
	CHECK(second.frame_index() == 0);
	CHECK(second.elapsed() == 0.0);
	second.update(0.25f);
	first.restart();
	CHECK(second.frame_index() == 1);
	CHECK(first.frame_index() == 0);
}

TEST_CASE("the total duration is computed in double instead of overflowing float")
{
	RenderResources resources;
	add_sheet(resources, "long", true, 4, std::numeric_limits<float>::max());
	AnimationPreview preview(resources, resources.resolve_sheet_strip("long", "preview"));
	preview.update(std::numeric_limits<float>::max());
	CHECK(preview.frame_index() == 1);
	CHECK(preview.elapsed() == static_cast<double>(std::numeric_limits<float>::max()));
	CHECK_FALSE(preview.finished());
}
