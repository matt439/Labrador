#include "samples/collision/entry_marker.h"

#include "engine/math/rectanglei.h"
#include "samples/collision/body.h"

using namespace labrador;

namespace collision
{
	EntryMarker::EntryMarker(const Vector2F& position, TextureHandle texture) :
		rectangle_(position.x, position.y, 12.0f, 12.0f), texture_(texture)
	{
	}

	void EntryMarker::update(float dt)
	{
		this->remaining_ -= dt;
		if (this->remaining_ <= 0.0f)
		{
			this->set_for_deletion(true);
		}
	}

	void EntryMarker::draw(DrawList& draw_list) const
	{
		draw_list.draw_sprite(this->texture_, RectangleI(0, 0, 1, 1),
			this->rectangle_, Colour::goldenrod, 0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
	}

	RectangleF EntryMarker::bounds() const { return this->rectangle_; }
	const Shape* EntryMarker::shape() const { return &this->rectangle_; }
	CollisionLayer EntryMarker::layer() const { return 0u; }
	CollisionMask EntryMarker::mask() const { return 0u; }
	CollisionTag EntryMarker::tag() const { return static_cast<CollisionTag>(BodyKind::entry_marker); }
	bool EntryMarker::for_deletion() const { return this->for_deletion_; }

	void EntryMarker::set_for_deletion(bool for_deletion)
	{
		this->for_deletion_ = for_deletion;
	}

	void EntryMarker::on_contact(const CollisionObject&, const Vector2F&, float)
	{
	}
}
