#pragma once

#include "engine/collision/collision_object.h"
#include "engine/render/renderer.h"

namespace collision
{
	// A trigger-entry indicator whose timer is game policy. Scene owns it;
	// nobody retains its pointer after the end_tick which retires it.
	class EntryMarker final : public labrador::CollisionObject
	{
	public:
		static constexpr float lifetime = 0.5f;

		EntryMarker(const labrador::Vector2F& position, labrador::TextureHandle texture);

		void update(float dt) override;
		void draw(labrador::DrawList& draw_list) const override;
		labrador::RectangleF bounds() const override;
		const labrador::Shape* shape() const override;
		labrador::CollisionLayer layer() const override;
		labrador::CollisionMask mask() const override;
		labrador::CollisionTag tag() const override;
		bool for_deletion() const override;
		void set_for_deletion(bool for_deletion) override;
		void on_contact(const labrador::CollisionObject& other,
			const labrador::Vector2F& normal, float penetration) override;

	private:
		labrador::RectangleF rectangle_;
		labrador::TextureHandle texture_;
		float remaining_ = lifetime;
		bool for_deletion_ = false;
	};
}
