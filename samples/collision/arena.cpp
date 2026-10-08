#include "samples/collision/arena.h"

#include <memory>

using namespace mattmath;
using namespace labrador;

namespace collisiondemo
{
	Arena::Arena(TextureHandle texture)
	{
		const RectangleF walls[] = {
			{ 40.0f, 140.0f, 1200.0f, 24.0f },
			{ 40.0f, 640.0f, 1200.0f, 24.0f },
			{ 40.0f, 164.0f, 24.0f, 476.0f },
			{ 1216.0f, 164.0f, 24.0f, 476.0f },
			{ 600.0f, 270.0f, 56.0f, 240.0f }
		};
		for (const RectangleF& wall : walls)
		{
			this->scene_.add(std::make_unique<Body>(BodyKind::wall, wall, texture));
		}
		this->scene_.add(std::make_unique<Body>(BodyKind::trigger,
			RectangleF(900.0f, 300.0f, 180.0f, 180.0f), texture));
		this->scene_.add(std::make_unique<Body>(BodyKind::decoration,
			RectangleF(340.0f, 300.0f, 100.0f, 180.0f), texture));
		this->player_ = this->scene_.add(std::make_unique<Body>(BodyKind::player,
			RectangleF(180.0f, 320.0f, 32.0f, 32.0f), texture));
		this->scene_.end_tick();
	}

	void Arena::step(Vector2F direction, float dt)
	{
		constexpr float move_speed = 240.0f;
		if (direction.length_squared() > 1.0f)
		{
			direction.normalize();
		}
		this->player_->set_velocity(direction * move_speed);
		this->scene_.update(dt);
		this->scene_.resolve();
		this->contact_count_ = this->scene_.contacts().size();
		this->hit_wall_ = this->hit_wall_ || this->player_->touching_wall();
		const bool in_trigger = this->player_->touching_trigger();
		if (in_trigger && !this->was_in_trigger_)
		{
			++this->trigger_entries_;
		}
		this->was_in_trigger_ = in_trigger;
		this->scene_.end_tick();
	}

	void Arena::reset()
	{
		this->player_->set_position(Vector2F(180.0f, 320.0f));
		this->player_->set_velocity(Vector2F::ZERO);
		this->player_->update(0.0f);
		this->contact_count_ = 0;
		this->trigger_entries_ = 0;
		this->was_in_trigger_ = false;
		this->hit_wall_ = false;
	}

	Body& Arena::player() { return *this->player_; }
	const Body& Arena::player() const { return *this->player_; }
	Scene& Arena::scene() { return this->scene_; }
	const Scene& Arena::scene() const { return this->scene_; }
	std::size_t Arena::contact_count() const { return this->contact_count_; }
	int Arena::trigger_entries() const { return this->trigger_entries_; }
	bool Arena::hit_wall() const { return this->hit_wall_; }
}
