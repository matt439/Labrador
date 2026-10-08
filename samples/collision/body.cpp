#include "samples/collision/body.h"

#include "engine/collision/resolve.h"
#include "engine/math/rectanglei.h"
#include "engine/render/renderer.h"

using namespace mattmath;
using namespace labrador;

namespace collisiondemo
{
	Body::Body(BodyKind kind, const RectangleF& rectangle, TextureHandle texture) :
		kind_(kind), rectangle_(rectangle), texture_(texture)
	{
	}

	CollisionLayer Body::layer() const
	{
		switch (this->kind_)
		{
		case BodyKind::player: return player_layer;
		case BodyKind::wall: return wall_layer;
		case BodyKind::trigger: return trigger_layer;
		case BodyKind::decoration: return decoration_layer;
		}
		return 0u;
	}

	CollisionMask Body::mask() const
	{
		switch (this->kind_)
		{
		case BodyKind::player: return wall_layer | trigger_layer | decoration_layer;
		case BodyKind::wall:
		case BodyKind::trigger: return player_layer;
		case BodyKind::decoration: return 0u;
		}
		return 0u;
	}

	void Body::update(float dt)
	{
		this->touching_wall_ = false;
		this->touching_trigger_ = false;
		this->rectangle_.x += this->velocity_.x * dt;
		this->rectangle_.y += this->velocity_.y * dt;
	}

	void Body::on_contact(const CollisionObject& other,
		const Vector2F& normal, float penetration)
	{
		if (this->kind_ != BodyKind::player)
		{
			return;
		}

		if (other.tag() == static_cast<CollisionTag>(BodyKind::wall))
		{
			const Vector2F movement = separation(normal, penetration);
			this->rectangle_.x += movement.x;
			this->rectangle_.y += movement.y;
			this->velocity_ = slide(this->velocity_, normal);
			this->touching_wall_ = true;
		}
		else if (other.tag() == static_cast<CollisionTag>(BodyKind::trigger))
		{
			this->touching_trigger_ = true;
		}
	}

	void Body::draw(DrawList& draw_list) const
	{
		Colour tint = Colour(70, 90, 115);
		switch (this->kind_)
		{
		case BodyKind::player:
			tint = this->touching_wall_ ? Colour::goldenrod : Colour(75, 190, 245);
			if (this->touching_trigger_)
			{
				tint = Colour(90, 235, 145);
			}
			break;
		case BodyKind::trigger: tint = Colour(24, 100, 66); break;
		case BodyKind::decoration: tint = Colour(88, 48, 116); break;
		case BodyKind::wall: break;
		}
		draw_list.draw_sprite(this->texture_, RectangleI(0, 0, 1, 1),
			this->rectangle_, tint, 0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);
	}

	RectangleF Body::bounds() const { return this->rectangle_; }
	const Shape* Body::shape() const { return &this->rectangle_; }
	CollisionTag Body::tag() const { return static_cast<CollisionTag>(this->kind_); }
	bool Body::for_deletion() const { return false; }
	Vector2F Body::velocity() const { return this->velocity_; }
	bool Body::touching_wall() const { return this->touching_wall_; }
	bool Body::touching_trigger() const { return this->touching_trigger_; }

	void Body::set_position(const Vector2F& position)
	{
		this->rectangle_.x = position.x;
		this->rectangle_.y = position.y;
	}

	void Body::set_velocity(const Vector2F& velocity)
	{
		this->velocity_ = velocity;
	}
}
