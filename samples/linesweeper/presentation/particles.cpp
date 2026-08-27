#include "samples/linesweeper/presentation/particles.h"

#include "engine/math/rectanglei.h"
#include "samples/linesweeper/presentation/layout.h"
#include "samples/linesweeper/presentation/palette.h"

#include <array>
#include <cstddef>

using namespace mattmath;
using namespace labrador;

namespace linesweeper
{
	namespace
	{
		// Pixels per second squared, downward. Screen y increases downward,
		// so gravity is positive - the same convention world.h picks for the
		// well and for the same reason: the rows and the rasteriser agree.
		constexpr float gravity = 900.0f;

		// A cleared row. The sparks go sideways out of the row rather than
		// up, because the row is what vanished and a horizontal spray is what
		// reads as a line leaving.
		constexpr int clear_particles_per_cell = 22;
		constexpr float clear_speed = 340.0f;
		constexpr float clear_lifetime = 0.85f;
		constexpr float clear_size = 5.0f;

		// A piece landing. Small, short, upward - a puff of dust off the top
		// of the four cells that just became stack.
		// Tuned by looking: six sparks of three and a half pixels for a third
		// of a second was invisible on screen, which is the whole reason the
		// drawing half of this sample is verified by running it.
		constexpr int lock_particles_per_cell = 10;
		constexpr float lock_speed = 150.0f;
		constexpr float lock_lifetime = 0.45f;
		constexpr float lock_size = 4.5f;

		// The top-out, and the number the field is sized around: a full well
		// is two hundred cells and this is what each of them costs.
		constexpr int shatter_particles_per_cell = 48;
		constexpr float shatter_speed = 420.0f;
		constexpr float shatter_lifetime = 1.6f;
		constexpr float shatter_size = 6.0f;

		// How bright a spark is at full life. Under premultiplied alpha a
		// tint with a = 0 adds without attenuating, so this is a multiplier on
		// what the background already has rather than a blend against it, and
		// overlapping sparks saturate towards white. That is what a burst
		// looks like and it is why it is not 1.0.
		constexpr float glow_intensity = 0.85f;

		// The board as it stood between the lock and the clear, which is the
		// one arrangement no World ever holds.
		//
		// A TICK LOCKS AND CLEARS TOGETHER, and that is the whole problem this
		// function exists to solve. tick.cpp writes the piece into the cells
		// and calls clear_lines in the same step, so a full row is never
		// visible from outside the simulation - not in last frame's match, not
		// in this frame's. A field looking for one finds none, ever.
		//
		// So it is reconstructed, out of the two pure queries world.h declares
		// for exactly this reason. shadow(before) is where a hard drop would
		// have put the falling piece, which is where it locked in every case
		// but one: gravity and soft drop lock a piece that is already resting,
		// so the shadow is the piece; a hard drop locks it at the shadow by
		// definition. The exception is a piece that moved sideways or rotated
		// on the very tick it locked, and the failure mode there is that no
		// row comes back full and no sparks are thrown. A missed burst on a
		// rare frame is a cost worth paying; a burst on the wrong row is not.
		//
		// THE EXACT ANSWER WAS PRICED AND REFUSED. A byte on World saying which
		// rows went would need four, because 277 bytes pads to 280 and both
		// sizeof and has_unique_object_representations fire (world.h). README,
		// The padding assert priced a rule out, is the same trade made once
		// already for a rule; this is it made again for an effect, and an
		// effect has even less claim on the value.
		std::array<std::uint8_t, well_columns * well_rows> board_at_lock(
			const World& before)
		{
			std::array<std::uint8_t, well_columns * well_rows> cells =
				before.cells;

			if (before.current.kind == Kind::none)
			{
				return cells;
			}

			const Piece landed = shadow(before);
			const std::array<Coord, piece_cell_count> occupied =
				piece_cells(landed);

			for (int index = 0; index < piece_cell_count; ++index)
			{
				if (!in_well(occupied[index].x, occupied[index].y))
				{
					continue;
				}

				cells[static_cast<std::size_t>(
					cell_index(occupied[index].x, occupied[index].y))] =
					static_cast<std::uint8_t>(landed.kind);
			}

			return cells;
		}

		// Whether every cell in a row of a reconstructed board is filled.
		bool row_full(
			const std::array<std::uint8_t, well_columns * well_rows>& cells,
			int y)
		{
			for (int x = 0; x < well_columns; ++x)
			{
				if (cells[static_cast<std::size_t>(cell_index(x, y))] ==
					cell_empty)
				{
					return false;
				}
			}

			return true;
		}
	}

	ParticleField::ParticleField(const World* world, TextureHandle block) :
		world_(world),
		previous_(*world),
		block_(block)
	{
		// previous_ is seeded from the live match rather than from World{},
		// because a field constructed against a match already in progress must
		// not read the whole board as having appeared this frame.
	}

	void ParticleField::update(float dt)
	{
		this->observe();
		this->previous_ = *this->world_;

		// THE TIGHT LOOP, and the only thing in this sample that runs ten
		// thousand times a frame.
		//
		// One linear pass over a dense prefix. A dead particle is overwritten
		// by the last live one and the count drops, so [0, count_) never
		// develops holes and neither this loop nor draw() ever tests a tombstone
		// - which is the whole reason the array is walked rather than a free
		// list chased. The swapped-in particle has not been stepped yet, so
		// the index deliberately does not advance.
		//
		// Order is not preserved and nothing wants it to be. Two sparks drawn
		// in the other order are the same picture, because they are additive.
		float min_x = 0.0f;
		float min_y = 0.0f;
		float max_x = 0.0f;
		float max_y = 0.0f;
		bool any = false;

		int index = 0;

		while (index < this->count_)
		{
			Particle& particle =
				this->particles_[static_cast<std::size_t>(index)];

			particle.velocity.y += gravity * dt;
			particle.position += particle.velocity * dt;
			particle.life -= particle.decay * dt;

			if (particle.life <= 0.0f)
			{
				--this->count_;
				particle =
					this->particles_[static_cast<std::size_t>(this->count_)];
				continue;
			}

			if (!any)
			{
				min_x = particle.position.x;
				min_y = particle.position.y;
				max_x = particle.position.x;
				max_y = particle.position.y;
				any = true;
			}
			else
			{
				min_x = particle.position.x < min_x ? particle.position.x
					: min_x;
				min_y = particle.position.y < min_y ? particle.position.y
					: min_y;
				max_x = particle.position.x > max_x ? particle.position.x
					: max_x;
				max_y = particle.position.y > max_y ? particle.position.y
					: max_y;
			}

			++index;
		}

		this->extent_ = any
			? RectangleF(min_x, min_y, max_x - min_x, max_y - min_y)
			: RectangleF(well_origin_x, well_origin_y, 0.0f, 0.0f);
	}

	void ParticleField::observe()
	{
		const World& now = *this->world_;
		const World& before = this->previous_;

		// A restart is `world = World{}`, so the tick ordinal goes backwards.
		// Everything on screen belonged to a match that no longer exists, and
		// clearing the field is one assignment to a counter for the same
		// reason restarting the match is one assignment to a value.
		if (now.tick < before.tick)
		{
			this->count_ = 0;
			return;
		}

		if (now.topped_out != 0 && before.topped_out == 0)
		{
			this->shatter();
			return;
		}

		// A CLEAR AND A LOCK ARE READ FROM THE SAME DIFFERENCE, and the clear
		// wins the frame they share.
		//
		// Clearing a row shifts everything above it down, so on a clear frame
		// most of the board's cells differ and a cell-by-cell diff says
		// nothing useful. The counter says a clear happened; the rows it
		// happened to come out of the board reconstructed at the moment
		// between the lock and the clear, which is the arrangement no World
		// holds - board_at_lock above is the whole argument. The lock is not
		// drawn separately on this frame because its four cells are inside the
		// rows that just exploded.
		if (now.lines > before.lines)
		{
			const std::array<std::uint8_t, well_columns * well_rows> locked =
				board_at_lock(before);

			for (int y = well_buffer_rows; y < well_rows; ++y)
			{
				if (!row_full(locked, y))
				{
					continue;
				}

				for (int x = 0; x < well_columns; ++x)
				{
					// Outward from the middle of the row, so the two halves
					// spray apart rather than everything drifting one way.
					const float sideways =
						static_cast<float>(x) < static_cast<float>(
							well_columns) * 0.5f ? -1.0f : 1.0f;

					this->burst(x, y,
						locked[static_cast<std::size_t>(cell_index(x, y))],
						clear_particles_per_cell, clear_speed,
						Vector2F(sideways, -0.35f), clear_lifetime,
						clear_size);
				}
			}

			return;
		}

		// No clear, so any cell that gained a colour is a cell the piece that
		// just locked left behind. Four of them, once every second or two.
		for (int y = well_buffer_rows; y < well_rows; ++y)
		{
			for (int x = 0; x < well_columns; ++x)
			{
				const std::size_t index =
					static_cast<std::size_t>(cell_index(x, y));

				if (before.cells[index] != cell_empty ||
					now.cells[index] == cell_empty)
				{
					continue;
				}

				this->burst(x, y, now.cells[index], lock_particles_per_cell,
					lock_speed, Vector2F(0.0f, -1.0f), lock_lifetime,
					lock_size);
			}
		}
	}

	void ParticleField::shatter()
	{
		const World& now = *this->world_;

		for (int y = well_buffer_rows; y < well_rows; ++y)
		{
			for (int x = 0; x < well_columns; ++x)
			{
				const std::uint8_t kind =
					now.cells[static_cast<std::size_t>(cell_index(x, y))];

				if (kind == cell_empty)
				{
					continue;
				}

				// Away from the middle of the well, so the stack comes apart
				// rather than collapsing into its own centre.
				const float sideways =
					static_cast<float>(x) - (static_cast<float>(well_columns) -
						1.0f) * 0.5f;

				this->burst(x, y, kind, shatter_particles_per_cell,
					shatter_speed, Vector2F(sideways * 0.22f, -0.8f),
					shatter_lifetime, shatter_size);
			}
		}
	}

	void ParticleField::burst(int x, int y, std::uint8_t kind, int count,
		float speed, const Vector2F& direction, float lifetime, float size)
	{
		const Vector2F centre = cell_centre(x, y);

		for (int index = 0; index < count; ++index)
		{
			// Somewhere inside the cell, not all from its exact middle: a
			// burst from one point reads as a firework and a burst from a
			// square reads as the square coming apart.
			const Vector2F position(
				centre.x + this->random_signed() * cell_size * 0.5f,
				centre.y + this->random_signed() * cell_size * 0.5f);

			// The aim, plus a spread of half the speed in both axes. Not an
			// angle and a magnitude, because that costs a sine and a cosine
			// per particle to produce a distribution nobody can distinguish
			// from this one at four pixels across.
			const Vector2F velocity =
				direction * (speed * (0.45f + 0.55f * this->random_unit())) +
				Vector2F(this->random_signed(), this->random_signed()) *
					(speed * 0.5f);

			this->emit(position, velocity, kind,
				lifetime * (0.55f + 0.45f * this->random_unit()), size);
		}
	}

	void ParticleField::emit(const Vector2F& position, const Vector2F& velocity,
		std::uint8_t kind, float lifetime, float size)
	{
		if (this->count_ >= particle_capacity)
		{
			++this->dropped_;
			return;
		}

		Particle& particle =
			this->particles_[static_cast<std::size_t>(this->count_)];

		particle.position = position;
		particle.velocity = velocity;
		particle.life = 1.0f;
		// Every caller multiplies a positive constant by a factor of at least
		// 0.55, so this never divides by zero and the field never inherits a
		// particle that cannot die.
		particle.decay = 1.0f / lifetime;
		particle.size = size;
		particle.kind = kind;

		++this->count_;
	}

	void ParticleField::draw(DrawList& draw_list) const
	{
		for (int index = 0; index < this->count_; ++index)
		{
			const Particle& particle =
				this->particles_[static_cast<std::size_t>(index)];

			// Shrinking as it fades is what makes a flat quad read as a spark
			// instead of a square, and it costs one multiply where a soft
			// radial texture would have cost an atlas. README, Particles,
			// records that the atlas the sample predicted it would need turned
			// out not to be needed for this.
			const float side = particle.size * (0.35f + 0.65f * particle.life);
			const float half = side * 0.5f;

			// Quadratic, so a spark holds its colour and then goes, which is
			// what a hot thing cooling looks like. Linear fades read as a
			// dimmer switch.
			draw_list.draw_sprite(this->block_, RectangleI(0, 0, 1, 1),
				RectangleF(particle.position.x - half,
					particle.position.y - half, side, side),
				glowing(kind_colour(static_cast<Kind>(particle.kind)),
					particle.life * particle.life * glow_intensity),
				0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);
		}
	}

	RectangleF ParticleField::bounds() const
	{
		return this->extent_;
	}

	std::uint32_t ParticleField::next_random()
	{
		this->rng_ += 0x9E3779B9u;

		std::uint32_t value = this->rng_;
		value = (value ^ (value >> 16)) * 0x21F0AAADu;
		value = (value ^ (value >> 15)) * 0x735A2D97u;

		return value ^ (value >> 15);
	}

	float ParticleField::random_unit()
	{
		// The top twenty-four bits over 2^24, which is every float in [0, 1)
		// that has a chance of being distinct. Using all thirty-two would ask
		// for precision a float has not got and round some of them to exactly
		// one.
		return static_cast<float>(this->next_random() >> 8) /
			static_cast<float>(1 << 24);
	}

	float ParticleField::random_signed()
	{
		return this->random_unit() * 2.0f - 1.0f;
	}
}
