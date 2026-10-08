#include "samples/local_multiplayer/arena.h"

#include "engine/math/rectanglei.h"

#include <algorithm>

using namespace labrador;
using namespace mattmath;

namespace multiplayer
{
	namespace
	{
		constexpr float move_speed = 240.0f;
		constexpr float player_half_size = 14.0f;
	}

	Arena::Arena(TextureHandle white) : white_(white) {}

	void Arena::set_directions(const std::array<Vector2F, player_count>& directions)
	{
		this->directions_ = directions;
		for (Vector2F& direction : this->directions_)
		{
			if (direction.length_squared() > 1.0f)
			{
				direction.normalize();
			}
		}
	}

	void Arena::update(float dt)
	{
		const RectangleF world = this->bounds();
		for (size_t player = 0; player < player_count; ++player)
		{
			Vector2F& position = this->positions_[player];
			position += this->directions_[player] * (move_speed * dt);
			position.x = std::clamp(position.x, world.left() + player_half_size,
				world.right() - player_half_size);
			position.y = std::clamp(position.y, world.top() + player_half_size,
				world.bottom() - player_half_size);
		}
	}

	RectangleF Arena::bounds() const
	{
		return RectangleF(0.0f, 0.0f, 1800.0f, 1080.0f);
	}

	Vector2F Arena::position(size_t player) const
	{
		return this->positions_.at(player);
	}

	Camera Arena::camera(size_t player, const Viewport& viewport) const
	{
		return Camera(this->position(player) - viewport.size() * 0.5f, 1.0f);
	}

	Colour Arena::player_colour(size_t player)
	{
		const std::array<Colour, player_count> colours = {
			Colour(55, 215, 235), Colour(255, 171, 67)
		};
		return colours.at(player);
	}

	void Arena::draw(DrawList& list) const
	{
		const auto rectangle = [this, &list](const RectangleF& bounds, const Colour& colour)
		{
			list.draw_sprite(this->white_, RectangleI(0, 0, 1, 1), bounds,
				colour, 0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);
		};

		const RectangleF world = this->bounds();
		rectangle(world, Colour(21, 30, 42));
		for (float x = 0.0f; x < world.width; x += 90.0f)
		{
			rectangle(RectangleF(x, 0.0f, 1.0f, world.height), Colour(38, 52, 66));
		}
		for (float y = 0.0f; y < world.height; y += 90.0f)
		{
			rectangle(RectangleF(0.0f, y, world.width, 1.0f), Colour(38, 52, 66));
		}
		// Shared landmarks make the two camera positions visible while moving.
		rectangle(RectangleF(880.0f, 490.0f, 40.0f, 100.0f), Colour(129, 103, 186));
		rectangle(RectangleF(850.0f, 520.0f, 100.0f, 40.0f), Colour(129, 103, 186));
		for (size_t player = 0; player < player_count; ++player)
		{
			rectangle(RectangleF(this->positions_[player], player_half_size, player_half_size),
				player_colour(player));
		}
	}
}
