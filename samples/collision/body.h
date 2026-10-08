#pragma once

#include "engine/collision/collision_object.h"
#include "engine/render/renderer.h"

namespace collisiondemo
{
	enum class BodyKind : labrador::CollisionTag
	{
		player,
		wall,
		trigger,
		decoration
	};

	constexpr labrador::CollisionLayer player_layer = 1u << 0;
	constexpr labrador::CollisionLayer wall_layer = 1u << 1;
	constexpr labrador::CollisionLayer trigger_layer = 1u << 2;
	constexpr labrador::CollisionLayer decoration_layer = 1u << 3;

	class Body final : public labrador::CollisionObject
	{
	public:
		Body(BodyKind kind, const mattmath::RectangleF& rectangle,
			labrador::TextureHandle texture);

		void update(float dt) override;
		void draw(labrador::DrawList& draw_list) const override;
		mattmath::RectangleF bounds() const override;
		const mattmath::Shape* shape() const override;
		labrador::CollisionLayer layer() const override;
		labrador::CollisionMask mask() const override;
		labrador::CollisionTag tag() const override;
		bool for_deletion() const override;
		void on_contact(const labrador::CollisionObject& other,
			const mattmath::Vector2F& normal, float penetration) override;

		void set_position(const mattmath::Vector2F& position);
		void set_velocity(const mattmath::Vector2F& velocity);
		mattmath::Vector2F velocity() const;
		bool touching_wall() const;
		bool touching_trigger() const;

	private:
		BodyKind kind_;
		mattmath::RectangleF rectangle_;
		labrador::TextureHandle texture_;
		mattmath::Vector2F velocity_ = mattmath::Vector2F::ZERO;
		bool touching_wall_ = false;
		bool touching_trigger_ = false;
	};
}
