#include <doctest/doctest.h>

#include "engine/core/game_object.h"
#include "engine/core/name_table.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/animation_strip.h"
#include "engine/render/colour.h"
#include "engine/render/render_resources.h"
#include "engine/render/sprite_frame.h"
#include "engine/render/sprite_sheet.h"
#include "engine/render/visual.h"

#include <cmath>
#include <memory>
#include <string>
#include <utility>

// The still sprite a scene can hold, and the one thing it adds over a
// TextureObject: a GameObject base with a bounds() a scene culls against.
//
// WHAT IS PINNED. bounds() is the whole of this file, because it is the whole
// of what a cull sees. It reported the destination rectangle, which is where
// a sprite lands only when its frame has no authored origin, the caller gave
// none, and nothing is turned - and a sprite meeting none of those was culled
// from a view it was drawn inside (docs/review/gpt6/README.md, G6-04). The
// arithmetic is sprite_geometry.h's and is held to the quad there; what these
// say is that Visual reaches it with the frame's origin and its own.

namespace
{
	using labrador::AnimationStrip;
	using labrador::GameObject;
	using labrador::NameTable;
	using labrador::RenderResources;
	using labrador::SpriteFrame;
	using labrador::SpriteSheet;
	using labrador::TextureHandle;
	using labrador::Visual;
	using mattmath::RectangleF;
	using mattmath::RectangleI;
	using mattmath::Vector2F;

	// Two frames of the same eight-by-eight source: one with no origin and
	// one whose authored origin is its own far corner, which is how a sheet
	// author pins a sprite by its bottom right.
	class Content
	{
	public:
		Content() : resources()
		{
			NameTable<SpriteFrame> frames("sprite frame");
			frames.add("plain", SpriteFrame(RectangleI(0, 0, 8, 8)));
			frames.add("pinned", SpriteFrame(RectangleI(0, 0, 8, 8),
				Vector2F(8.0f, 8.0f)));
			this->resources.add_sprite_sheet("sheet",
				std::make_unique<SpriteSheet>(TextureHandle(),
					std::move(frames), NameTable<AnimationStrip>("strip")));
		}

		RenderResources resources;
	};

	bool close(const RectangleF& left, const RectangleF& right)
	{
		const float tolerance = 0.001f;
		return std::fabs(left.x - right.x) < tolerance &&
			std::fabs(left.y - right.y) < tolerance &&
			std::fabs(left.width - right.width) < tolerance &&
			std::fabs(left.height - right.height) < tolerance;
	}
}

TEST_CASE("a plain sprite's box is its rectangle")
{
	Content content;
	const Visual visual("sheet", "plain",
		RectangleF(100.0f, 20.0f, 20.0f, 20.0f), &content.resources);

	const GameObject& object = visual;
	CHECK(object.bounds() == RectangleF(100.0f, 20.0f, 20.0f, 20.0f));
}

TEST_CASE("an authored frame origin moves the box the way it moves the sprite")
{
	// The caller wrote no origin. The sheet did, and the sprite lands with
	// its far corner at the rectangle's near one: x=80..100, not 100..120.
	Content content;
	const Visual visual("sheet", "pinned",
		RectangleF(100.0f, 20.0f, 20.0f, 20.0f), &content.resources);

	CHECK(close(visual.bounds(), RectangleF(80.0f, 0.0f, 20.0f, 20.0f)));
}

TEST_CASE("the caller's origin adds to the frame's")
{
	// Four texels of the caller's on top of the frame's eight, over a
	// twenty-pixel destination: twelve texels is thirty pixels of shift.
	Content content;
	const Visual visual("sheet", "pinned",
		RectangleF(100.0f, 20.0f, 20.0f, 20.0f), &content.resources,
		labrador::Colour::white, 0.0f, Vector2F(4.0f, 4.0f));

	CHECK(close(visual.bounds(), RectangleF(70.0f, -10.0f, 20.0f, 20.0f)));
}

TEST_CASE("a turned sprite's box is the box around the turned rectangle")
{
	// The review's second row: a half turn about the top left puts the whole
	// sprite up and left of it.
	Content content;
	const float HALF_TURN = 3.14159265f;
	const Visual visual("sheet", "plain",
		RectangleF(100.0f, 20.0f, 20.0f, 20.0f), &content.resources,
		labrador::Colour::white, HALF_TURN);

	CHECK(close(visual.bounds(), RectangleF(80.0f, 0.0f, 20.0f, 20.0f)));
}

TEST_CASE("a visual has nothing to step")
{
	Content content;
	Visual visual("sheet", "pinned",
		RectangleF(100.0f, 20.0f, 20.0f, 20.0f), &content.resources);

	const RectangleF before = visual.bounds();
	visual.update(1.0f / 60.0f);
	CHECK(visual.bounds() == before);
}
