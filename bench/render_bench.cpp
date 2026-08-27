#include "bench/bench.h"

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/renderer.h"
#include "engine/render/sprite_geometry.h"
#include "engine/render/sprite_vertex.h"

#include <vector>

// The draw path's arithmetic, which nothing measured until this file.
//
// PHILOSOPHY's Performance section names throughput on the render path as the
// measure, and the phases asserted on beside it were Scene::update,
// Scene::end_tick, a hand-copied cull and Scene::resolve - four phases of the
// simulation and none of the drawing. What a frame actually spends its
// per-sprite time on is engine/render/sprite_geometry.cpp: four corners, a
// rotation, a flip and a texel-to-uv divide, once per sprite, for every
// backend.
//
// IT IS THE ENGINE'S HALF ONLY, AND THAT IS WHY IT BUILDS EVERYWHERE. No
// Renderer is constructed and no device is created, so this compiles and runs
// under all five render backends including null - which is the reason to
// measure here rather than through Renderer::draw_sprite. The number it
// produces is a floor and not a frame time: what a backend adds - the buffer
// map, the upload, the draw call - is not in it and must not be read out of
// it. Half of what PHILOSOPHY asks for is still unmeasured after this file,
// and saying so here is cheaper than someone quoting the figure later as
// though it were the cost of drawing.
//
// WHAT THE FIGURE PRICES BESIDES THE ARITHMETIC. Both entry points are called
// across a translation unit boundary, so are the Vector2F operators the corner
// loop inside them uses (engine/math/vector2f.cpp), and this build turns on no
// cross-module optimisation - so the number prices those call boundaries along
// with the maths. That is the honest thing to measure, because a frame reaches
// build_sprite_quad across exactly the same boundaries. It does mean the
// figure is a property of how this tree is linked as much as of what the
// function computes, and a build that inlined them would report a different
// floor without a line of arithmetic changing.
//
// THE OBJECT IN THE HARNESS'S "ns/object" COLUMN IS A SPRITE. bench.h counts
// whatever n the caller passed, and n here is the sprite count rather than the
// vertex count - so whatever the column says is the cost of all four corners
// of one sprite, not of one of them.

using labrador::Colour;
using labrador::SpriteFlip;
using labrador::SpriteVertex;
using mattmath::RectangleF;
using mattmath::RectangleI;
using mattmath::Vector2F;

namespace
{
	// One atlas, one frame in it, and every sprite drawing that frame. That is
	// the shape a batch out of a single texture has, and it is the shape the
	// particle field docs/survey/2026-08-26.md 3.2 proposes would arrive in.
	constexpr float atlas_side = 512.0f;
	constexpr float frame_side = 32.0f;

	// A grid, for the reason scene_bench.cpp gives for its own: a random
	// scatter would put the random number generator's cache behaviour inside
	// the measurement. A fixed row width rather than a square, because nothing
	// here does a spatial query - the layout exists to give every sprite
	// different inputs, not to be searched.
	constexpr int row_width = 128;
	constexpr float cell = 64.0f;
	constexpr float size = 48.0f;

	// build_scaled_quad takes a scale over the source where build_sprite_quad
	// takes a destination rectangle, so this is what makes the two draw the
	// same size - and therefore what makes the two numbers comparable.
	constexpr float scale = size / frame_side;

	const RectangleI frame(64, 128,
		static_cast<int>(frame_side), static_cast<int>(frame_side));
	const Vector2F texture_size(atlas_side, atlas_side);

	// Centred, because that is the point a particle rotates about - and
	// because an origin of zero would leave origin_ratio at zero inside
	// build_quad, which is the one input where the corner it is subtracted
	// from does not move.
	const Vector2F origin(frame_side / 2.0f, frame_side / 2.0f);

	// What a particle varies from its neighbours, and nothing else: the atlas,
	// the frame and the origin are the same for every one of them.
	struct SpriteCall
	{
		RectangleF destination;
		Colour tint;
		float rotation = 0.0f;
	};

	std::vector<SpriteCall> build_calls(int count, bool rotated)
	{
		std::vector<SpriteCall> calls;
		calls.reserve(static_cast<size_t>(count));

		for (int i = 0; i < count; i++)
		{
			SpriteCall call;

			// FRACTIONAL ON PURPOSE. build_sprite_quad truncates each of its
			// four edges to a whole pixel, and a grid laid out on whole pixels
			// would time those four std::trunc calls on the one input where
			// they have nothing to do.
			call.destination = RectangleF(
				static_cast<float>(i % row_width) * cell + 0.5f,
				static_cast<float>(i / row_width) * cell + 0.25f,
				size, size);

			// Fading out, which is what a particle field does to its tint.
			call.tint = Colour(1.0f, 1.0f, 1.0f,
				1.0f - static_cast<float>(i % 64) / 64.0f);

			// Never zero when rotated, because zero is the other side of the
			// branch build_quad takes on the angle and the two cases exist to
			// be compared. The cycle length is prime and the row width is not,
			// so the angle does not repeat down a column and the sine and the
			// cosine are asked a fresh question each sprite.
			call.rotation = rotated
				? 0.05f + static_cast<float>(i % 61) * 0.1f
				: 0.0f;

			calls.push_back(call);
		}
		return calls;
	}

	// Keeps the vertices from being dead stores.
	//
	// Nothing here can be elided today - both entry points are in another
	// translation unit and this build turns on no cross-module optimisation
	// (cmake/settings.cmake) - so this is the guard against that stopping
	// being true, and it costs one volatile store a repetition rather than one
	// a sprite.
	volatile float sink = 0.0f;

	void build_sprite_quads(const std::vector<SpriteCall>& calls,
		std::vector<SpriteVertex>& vertices)
	{
		for (size_t i = 0; i < calls.size(); i++)
		{
			labrador::build_sprite_quad(calls[i].destination, frame,
				texture_size, calls[i].tint, calls[i].rotation, origin,
				SpriteFlip::none, &vertices[i * 4]);
		}
		sink = vertices.back().position.x;
	}

	void build_scaled_quads(const std::vector<SpriteCall>& calls,
		std::vector<SpriteVertex>& vertices)
	{
		for (size_t i = 0; i < calls.size(); i++)
		{
			labrador::build_scaled_quad(calls[i].destination.top_left(), scale,
				frame, texture_size, calls[i].tint, calls[i].rotation, origin,
				&vertices[i * 4]);
		}
		sink = vertices.back().position.x;
	}
}

void run_render_benchmarks()
{
	// WHY THESE START WHERE scene_bench.cpp's COUNTS STOP. The counts that
	// matter here are not a scene's. A shipped frame's board is two hundred
	// cells - LineSweeper's well is ten columns by twenty visible rows
	// (samples/linesweeper/rules/world.h) - and the particle field
	// docs/survey/2026-08-26.md 3.2 proposes is ten thousand, so this span
	// brackets the largest count anybody has asked for rather than stopping
	// short of it.
	//
	// It is also the span over which the only thing that can bend this curve
	// appears. Nothing here looks anything up and nothing allocates, so a
	// sprite's cost can only come to depend on the sprite count through the
	// memory it touches - and across these four runs the vertices grow from
	// comfortably inside a cache to comfortably outside every one of them.
	//
	// The span is 64x, which is the span main.cpp's 8x slack was chosen
	// against.
	const int counts[] = { 1024, 4096, 16384, 65536 };

	for (int count : counts)
	{
		const std::vector<SpriteCall> upright = build_calls(count, false);
		const std::vector<SpriteCall> turned = build_calls(count, true);
		std::vector<SpriteVertex> vertices(static_cast<size_t>(count) * 4);

		// The unrotated case, which is the one a shipped frame is made of:
		// LineSweeper's single draw_sprite call passes an angle of zero
		// (samples/linesweeper/presentation/board_view.cpp).
		bench::record(bench::run("build_sprite_quad", count,
			[&upright, &vertices] { build_sprite_quads(upright, vertices); }));

		// The same work with the angle branch taken, which is the one claim
		// sprite_geometry.cpp makes about its own speed. main.cpp prints what
		// the difference came to.
		bench::record(bench::run("build_sprite_quad (rotated)", count,
			[&turned, &vertices] { build_sprite_quads(turned, vertices); }));

		// The path text is laid out by, on the same placements and therefore
		// on the same amount of work bar one term: this one multiplies the
		// source size by a scale where the first truncates four edges and
		// subtracts. The gap between the two is what that truncation costs.
		bench::record(bench::run("build_scaled_quad", count,
			[&upright, &vertices] { build_scaled_quads(upright, vertices); }));
	}
}
