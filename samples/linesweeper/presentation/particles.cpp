#include "samples/linesweeper/presentation/particles.h"

#include "engine/math/rectanglei.h"
#include "samples/linesweeper/presentation/layout.h"
#include "samples/linesweeper/presentation/palette.h"

#include <algorithm>
#include <array>
#include <cstddef>

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
		//
		// Slow and long, as well as cool (shatter_heat, particles.h): the
		// stack crumbles and settles rather than bursting. Brightness is the
		// term the flash check moves with; the speed and lifetime are what
		// keep a spark that dim on screen long enough to be seen.
		constexpr int shatter_particles_per_cell = 48;
		constexpr float shatter_speed = 160.0f;
		constexpr float shatter_lifetime = 2.2f;
		constexpr float shatter_size = 6.0f;

		// How bright a spark is at a heat of one. Under premultiplied alpha a
		// tint with a = 0 adds without attenuating, so this is a multiplier on
		// what the background already has rather than a blend against it, and
		// overlapping sparks saturate towards white. That is what a burst
		// looks like and it is why it is not 1.0 - and it is also why each
		// burst's heat is a limit (particles.h): saturating towards white is
		// a flash when it happens over enough of the screen.
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
		// So it is rebuilt: last frame's cells, plus the piece as it locked,
		// which is the one value tick() hands back (world.h, TickResult). Not
		// shadow(before) - where a hard drop would have put last frame's
		// piece: a horizontal I rotated and dropped in one tick clears row 21
		// while the shadow of the horizontal one fills row 20, which would
		// throw sparks out of a row that is still there. The exact piece
		// costs the World nothing, which is why it is a return value and not
		// a member.
		std::array<std::uint8_t, well_columns * well_rows> board_at_lock(
			const World& before, const Piece& landed)
		{
			std::array<std::uint8_t, well_columns * well_rows> cells =
				before.cells;

			if (landed.kind == Kind::none)
			{
				return cells;
			}

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

	ParticleField::ParticleField(const World* world,
		const TickResult* last_tick, TextureHandle block,
		const StepPosition* step, Motion motion, const Palette* palette) :
		world_(world),
		last_tick_(last_tick),
		motion_(motion),
		palette_(palette),
		step_(step),
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

		if (this->step_ != nullptr)
		{
			this->stepped_at_ = this->step_->step;
		}
		this->step_seconds_ = dt;

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

			// Both where the spark is and where draw() could carry it before
			// the next step, which is less than a whole step along its
			// velocity (lead).
			const float half = particle.size * (0.35f + 0.65f * particle.life) * 0.5f;
			const Vector2F ahead = particle.position + particle.velocity * dt;
			const float left = std::min(particle.position.x, ahead.x) - half;
			const float top = std::min(particle.position.y, ahead.y) - half;
			const float right = std::max(particle.position.x, ahead.x) + half;
			const float bottom = std::max(particle.position.y, ahead.y) + half;
			if (!any)
			{
				min_x = left;
				min_y = top;
				max_x = right;
				max_y = bottom;
				any = true;
			}
			else
			{
				min_x = std::min(min_x, left);
				min_y = std::min(min_y, top);
				max_x = std::max(max_x, right);
				max_y = std::max(max_y, bottom);
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
		// happened to come out of the board rebuilt at the moment between the
		// lock and the clear, which is the arrangement no World holds -
		// board_at_lock above is the whole argument, and the piece it takes
		// is the one the tick that cleared them locked. The lock is not drawn
		// separately on this frame because its four cells are inside the rows
		// that just exploded.
		if (now.lines > before.lines)
		{
			const std::array<std::uint8_t, well_columns * well_rows> locked =
				board_at_lock(before, this->last_tick_->locked);

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
						clear_size, clear_heat);
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
					lock_size, lock_heat);
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
					shatter_lifetime, shatter_size, shatter_heat);
			}
		}
	}

	void ParticleField::burst(int x, int y, std::uint8_t kind, int count,
		float speed, const Vector2F& direction, float lifetime, float size,
		float heat)
	{
		const Vector2F centre = cell_centre(x, y);
		const int sparks = sparks_per_cell(count, this->motion_);

		for (int index = 0; index < sparks; ++index)
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
				lifetime * (0.55f + 0.45f * this->random_unit()), size, heat);
		}
	}

	void ParticleField::emit(const Vector2F& position, const Vector2F& velocity,
		std::uint8_t kind, float lifetime, float size, float heat)
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
		particle.life = heat;
		// Every caller multiplies a positive constant by a factor of at least
		// 0.55, so this never divides by zero and the field never inherits a
		// particle that cannot die. Heat over lifetime, so a spark born cooler
		// still lives as long as it was asked to.
		particle.decay = heat / lifetime;
		particle.size = size;
		particle.kind = kind;

		++this->count_;
	}

	float ParticleField::lead() const
	{
		if (this->step_ == nullptr || this->step_->step != this->stepped_at_)
		{
			return 0.0f;
		}

		return this->step_->fraction * this->step_seconds_;
	}

	void ParticleField::draw(DrawList& draw_list) const
	{
		// Along the velocity and not the curve: gravity's share of less than
		// one step is under a pixel at any speed this field throws, and the
		// next update draws the curve's true point.
		const float lead = this->lead();

		// Once a frame, not once a spark: the selection only changes between
		// frames (palette.h).
		const Palette palette = *this->palette_;

		for (int index = 0; index < this->count_; ++index)
		{
			const Particle& particle =
				this->particles_[static_cast<std::size_t>(index)];
			const Vector2F position = particle.position + particle.velocity * lead;

			// Shrinking as it fades is what makes a flat quad read as a spark
			// instead of a square, and it costs one multiply where a soft
			// radial texture would have cost an atlas. README, The particle
			// field, records that the atlas the sample predicted it would need
			// turned out not to be needed for this.
			const float side = particle.size * (0.35f + 0.65f * particle.life);
			const float half = side * 0.5f;

			// Quadratic, so a spark holds its colour and then goes, which is
			// what a hot thing cooling looks like. Linear fades read as a
			// dimmer switch.
			draw_list.draw_sprite(this->block_, RectangleI(0, 0, 1, 1),
				RectangleF(position.x - half, position.y - half, side, side),
				glowing(kind_colour(static_cast<Kind>(particle.kind), palette),
					particle.life * particle.life * glow_intensity),
				0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
		}
	}

	float ParticleField::hottest() const
	{
		float hottest = 0.0f;

		for (int index = 0; index < this->count_; ++index)
		{
			hottest = std::max(hottest,
				this->particles_[static_cast<std::size_t>(index)].life);
		}

		return hottest;
	}

	RectangleF ParticleField::bounds() const
	{
		return this->extent_;
	}

	RectangleF ParticleField::cull_bounds(float units_per_pixel) const
	{
		// These quads have neither rotation nor origin: each view-space edge
		// truncates by less than one pixel. Empty fields stay empty.
		if (this->count_ == 0) return this->extent_;
		return RectangleF(this->extent_.x - units_per_pixel,
			this->extent_.y - units_per_pixel,
			this->extent_.width + 2.0f * units_per_pixel,
			this->extent_.height + 2.0f * units_per_pixel);
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
