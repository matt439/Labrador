#pragma once

#include "engine/collision/collision_layer.h"
#include "engine/collision/collision_object.h"
#include "engine/math/rectanglef.h"
#include "engine/math/shape.h"
#include "engine/math/vector2f.h"

// A collision object that is nothing but a rectangle, its layer, mask and
// tag, and a deletion flag.
//
// CollisionObject's pure virtual surface is written here once, so a change to
// it is a change to this file rather than to every test that needs a
// collider. Each method does the least it can: update, draw and on_contact do
// nothing, and set_for_deletion records the request. A test that needs more -
// a contact log, a marker it draws, an object that refuses deletion -
// derives from this and overrides that one method.
class StubCollider : public labrador::CollisionObject
{
public:
	StubCollider(const labrador::RectangleF& rectangle,
		labrador::CollisionLayer layer, labrador::CollisionMask mask,
		labrador::CollisionTag tag = 0) :
		rectangle_(rectangle), layer_(layer), mask_(mask), tag_(tag)
	{
	}

	void update(float /*dt*/) override {}
	void draw(labrador::DrawList& /*draw_list*/) const override {}
	labrador::RectangleF bounds() const override { return this->rectangle_; }

	const labrador::Shape* shape() const override { return &this->rectangle_; }
	labrador::CollisionLayer layer() const override { return this->layer_; }
	labrador::CollisionMask mask() const override { return this->mask_; }
	labrador::CollisionTag tag() const override { return this->tag_; }

	void on_contact(const labrador::CollisionObject& /*other*/,
		const labrador::Vector2F& /*normal*/, float /*penetration*/) override
	{
	}

	bool for_deletion() const override { return this->for_deletion_; }
	void set_for_deletion(bool for_deletion) override
	{
		this->for_deletion_ = for_deletion;
	}

	const labrador::RectangleF& rectangle() const { return this->rectangle_; }

protected:
	labrador::RectangleF rectangle_;

private:
	labrador::CollisionLayer layer_ = 0;
	labrador::CollisionMask mask_ = 0;
	labrador::CollisionTag tag_ = 0;
	bool for_deletion_ = false;
};
