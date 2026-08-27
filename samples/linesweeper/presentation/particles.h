#pragma once

#include "engine/core/game_object.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/renderer.h"
#include "samples/linesweeper/rules/world.h"

#include <array>
#include <cstdint>
#include <type_traits>

// Ten thousand particles, and one GameObject holding all of them.
//
// THIS IS THE HALF OF THE SAMPLE THE README SAID WAS MISSING. `World` is the
// value-semantics demonstration - 276 bytes, restart is an assignment, a
// replay is a memcmp. This file is the data-layout one, and PHILOSOPHY, The
// object model, is the claim it exists to make:
//
//   "Both ends of the spectrum are first-class. A game can be written in full
//    OOP - one registered object per entity - or for full performance: one
//    registered object standing for thousands of values updated in a tight
//    loop. Interface granularity is the user's performance dial."
//
// Nothing in this repository stood at that end until this file did. The scene
// holds ONE object here, not ten thousand: one update(), one draw(), one
// bounds(), and three virtual calls a frame whatever the particle count. Ten
// thousand GameObjects would be ten thousand virtual updates, ten thousand
// heap allocations, ten thousand bounds() against the cull, and a scene whose
// object list is a pointer chase - which is the shape this paragraph of
// PHILOSOPHY exists to say a game never has to adopt.
//
// IT ASKED THE ENGINE FOR NOTHING. No blend mode, no instancing verb, no
// particle system, no new backend state and no golden image: ten thousand
// draw_sprite calls through the one verb renderer.h already had, from inside
// one draw(). An engine-side particle system would have been the speculative
// framework T1 rules out - the mechanism the engine owes a game here is a
// batched sprite draw, and it already owned it.
//
// WHAT IT COSTS, PRICED BEFORE IT WAS WRITTEN. bench/render_bench.cpp puts the
// engine's quad arithmetic at 35.4 ns a sprite on this desktop, flat from a
// thousand sprites to sixty-five thousand. Ten thousand of them is therefore
// about 354 microseconds of arithmetic in a 16.7 ms frame - a fiftieth of the
// budget - and that number is why this file does not bother with a spatial
// index, a sort, or a second submission path. README, Particles, records what
// it actually measured against that prediction.
namespace linesweeper
{
	// The whole field, and it is a compile-time bound rather than a capacity
	// that grows.
	//
	// Ten thousand because that is what a top-out costs: the well is ten by
	// twenty and a full one disintegrates at forty-eight particles a cell,
	// which is 9,600 of them in one frame. The four hundred left over is the
	// headroom, and a burst that overruns it drops the NEWEST rather than
	// recycling the oldest - see ParticleField::dropped(). Dropping the oldest
	// would make each burst eat the one before it, and the visible difference
	// between 10,000 sparks and 9,600 is nothing anybody can see.
	inline constexpr int particle_capacity = 10000;

	// One particle. Thirty-two bytes, and every one of them is read by both
	// halves of the frame.
	//
	// AoS AND NOT SoA, WHICH IS THE DECISION WORTH ARGUING. The reflex for
	// "laid out for the cache" is structure-of-arrays, and it pays when a pass
	// touches a subset of the fields - a physics step that only wants position
	// and velocity streams half as much memory if colour and lifetime live
	// elsewhere. That is not this loop. update() reads and writes position,
	// velocity, life and decay, and draw() reads position, life, size and
	// kind; between them they touch every field of every live particle every
	// frame. Splitting them would buy nothing, cost six pointers where there
	// is one array, and make each of the two loops walk six streams instead of
	// one.
	//
	// So the layout that matters here is the size, and thirty-two is chosen:
	// the array is exactly two particles to a sixty-four byte cache line and
	// no particle ever straddles one. Twenty-four bytes was reachable by
	// packing decay, size and kind into indices behind lookup tables, and is
	// declined on T3 - a sample meant to be READ does not trade three plain
	// floats for a table nobody can check by eye, to win a fraction of a
	// microsecond on the fiftieth of the frame this already fits in.
	//
	// THE THREE SPARE BYTES ARE PADDING AND THAT IS FINE HERE, which is worth
	// saying next to a rules layer that asserts it has none. World's
	// no-padding contract exists because tests/linesweeper/ compares two
	// matches with memcmp and padding bits make that undefined. Nothing
	// compares two particles. A field is not a match: it carries no rule, it
	// is absent from the recording, and two replays of the same inputs are the
	// same match with entirely different sparks over them.
	struct Particle
	{
		// Back-buffer pixels, and pixels per second. The presentation is the
		// half of this sample allowed to be smooth - every duration in rules/
		// is counted in fixed ticks and none in seconds, and every duration
		// here is the other way round.
		mattmath::Vector2F position;
		mattmath::Vector2F velocity;

		// One at birth, zero at death, and it is the alpha as well as the
		// clock. Counting down from one rather than up to a lifetime means
		// draw() needs no division to find the fade, which is the only
		// arithmetic ten thousand of anything should be asked to avoid.
		float life = 0.0f;

		// Life lost per second, so a lifetime is spelt once at emission and
		// never stored.
		float decay = 0.0f;

		// The side of the quad, in pixels, at full life.
		float size = 0.0f;

		// The rules' own cell byte: 0 empty, 1..7 the piece that filled it.
		// Not a Colour, which is four floats and would take this to forty-eight
		// bytes on its own. The presentation colours a byte here for exactly
		// the reason world.h gives for storing one - a colour is not a rule,
		// and palette.h is the one place that turns the byte into light.
		std::uint8_t kind = 0;

		std::uint8_t spare[3] = {};
	};

	static_assert(sizeof(Particle) == 32,
		"A particle is two to a cache line, and the update loop's whole "
		"argument is that it streams. If this fired because you added a "
		"member, the question is whether it earns the line it just split.");
	static_assert(std::is_trivially_copyable_v<Particle>,
		"Compaction moves a particle by assigning it over a dead one. A "
		"member that needs more than that is a member with an owner "
		"somewhere else.");

	// The field: one object, one array, and no allocation after construction.
	//
	// IT LEARNS WHAT HAPPENED BY KEEPING LAST FRAME'S MATCH AND LOOKING, and
	// that is the sentence this whole class exists to be able to write. There
	// is no event queue, no callback, no observer list and not one line in
	// rules/ that knows this file exists - the field holds a World by value,
	// compares it with the live one every update(), and reads the clears, the
	// locks and the top-out straight out of the difference.
	//
	// That is only affordable because a match is 276 trivially copyable bytes.
	// An object graph would have needed the event bus; a value this size makes
	// the bus redundant, and the presentation stays a pure reader of the
	// simulation instead of something the simulation has to notify. It is the
	// same argument README makes for restart being an assignment, arriving
	// from the other side.
	//
	// THE LAYER RULE, AND THIS FILE OBEYS THE SAME HALF board_view.h DOES: it
	// includes rules/world.h - the value - and does not include rules/tick.h.
	// There is no way to step a match from here, so a particle can be a
	// consequence of the simulation and never a cause of one.
	class ParticleField final : public labrador::GameObject
	{
	public:
		// A borrowed match and a resolved handle - NOT the resource table
		// board_view.h takes, and the difference is what makes this the one
		// drawable in the sample that can be stepped with no device.
		//
		// BoardView needs the table because it measures text, and measuring
		// walks a font atlas. A particle needs one texture handle, which is an
		// integer: nothing in update() touches it, so a field built with an
		// unresolved handle simulates ten thousand particles perfectly well
		// and only draw() would object. That is why tests/linesweeper/ can
		// assert on this file and cannot assert on the other one, and it is
		// the sample's own version of the rule that a seam ships with a
		// headless implementation (PHILOSOPHY, Tests and toolchain).
		//
		// The state spells the asset name and resolves it, the same way it
		// spells the font name for the labels beside this (play_state.cpp).
		ParticleField(const World* world, labrador::TextureHandle block);

		// Reads the difference against last frame's match, emits whatever it
		// finds, then integrates and compacts. One pass, one thread.
		void update(float dt) override;

		// Ten thousand quads through the one verb the seam has, and not one
		// member written - draw() is const all the way down because several
		// view workers enter it on this same object at once (game_object.h).
		void draw(labrador::DrawList& draw_list) const override;

		// The live particles' own extent, computed in update() and stored.
		//
		// Measured rather than declared as the whole screen, because that is
		// the one thing this object can report cheaply that a cull would
		// actually use: a field with nothing in it is a box with no area, and
		// on the frames a burst is alive the box is the burst. It costs the
		// update loop four comparisons a particle and it costs draw() nothing,
		// which is the right side of the split for it to be on.
		mattmath::RectangleF bounds() const override;

		// How many particles are alive. The array's live prefix is [0, live()).
		int live() const
		{
			return this->count_;
		}

		// How many emissions have been refused because the field was full,
		// since construction.
		//
		// It is public because a drop that nothing can observe is the T6 trap
		// this sample keeps naming: silently doing less than asked. Nothing in
		// the sample reads it today - the HUD that would is the instrument
		// panel, still deferred - but tests/linesweeper/ asserts on it, which
		// is the difference between a documented policy and a hope.
		std::uint32_t dropped() const
		{
			return this->dropped_;
		}

	private:
		// The difference between last frame's match and this one, as bursts.
		void observe();

		// Every filled cell of the well, thrown outward. A top-out.
		void shatter();

		// One cell's worth of sparks, aimed by `direction` and spread around
		// it.
		void burst(int x, int y, std::uint8_t kind, int count, float speed,
			const mattmath::Vector2F& direction, float lifetime, float size);

		// Appends one particle, or counts a drop. The only place count_ grows.
		void emit(const mattmath::Vector2F& position,
			const mattmath::Vector2F& velocity, std::uint8_t kind,
			float lifetime, float size);

		// SplitMix32, the same mixer rules/tick.cpp deals pieces with, and
		// deliberately a SECOND stream rather than a share of that one. The
		// field may not touch World::rng: it holds a const World* and could
		// not, and if it could, a spark would change which piece came next and
		// the recording would stop being a function of its inputs. Sparks are
		// allowed to differ between two replays of the same match. Pieces are
		// not.
		std::uint32_t next_random();
		float random_unit();
		float random_signed();

		const World* world_ = nullptr;

		// Last frame's match, by value. The whole event system.
		World previous_{};

		// 320 KB, one allocation, at construction. Per-frame code performs no
		// heap allocation (PHILOSOPHY, Performance), and a particle field is
		// the phase that would otherwise do the most of it.
		std::array<Particle, particle_capacity> particles_{};

		int count_ = 0;
		std::uint32_t dropped_ = 0;
		std::uint32_t rng_ = 0;

		mattmath::RectangleF extent_;

		labrador::TextureHandle block_;
	};
}
