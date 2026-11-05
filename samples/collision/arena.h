#pragma once

#include "samples/collision/body.h"

#include "engine/scene/scene.h"

#include <cstddef>

namespace collision
{
	class Arena
	{
	public:
		// An unresolved texture is useful for headless simulation. Drawing
		// requires a handle resolved against the renderer's resource table.
		explicit Arena(labrador::TextureHandle texture = {});

		// Where the player's top-left corner starts, and where reset() puts it
		// back; the player is player_size units square.
		static constexpr labrador::Vector2F player_start{ 180.0f, 320.0f };
		static constexpr float player_size = 32.0f;

		void step(labrador::Vector2F direction, float dt);
		void reset();
		Body& player();
		const Body& player() const;
		labrador::Scene& scene();
		const labrador::Scene& scene() const;
		std::size_t contact_count() const;
		int trigger_entries() const;
		bool hit_wall() const;

	private:
		labrador::Scene scene_{ nullptr, nullptr };
		// Borrowed from scene_, which owns and retains the player for its life.
		Body* player_ = nullptr;
		std::size_t contact_count_ = 0;
		int trigger_entries_ = 0;
		bool was_in_trigger_ = false;
		bool hit_wall_ = false;
	};
}
