#include "samples/local_multiplayer/play_state.h"

#include "samples/local_multiplayer/controls.h"

#include "engine/render/resource_factory.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

using namespace labrador;

namespace multiplayer
{
	namespace
	{
		// The smoke test's route: player 1 right and player 2 up, every tick.
		const std::array<Vector2F, Arena::player_count> smoke_directions = {
			Vector2F(1.0f, 0.0f), Vector2F(0.0f, -1.0f)
		};
		constexpr float hud_height = 68.0f;
		constexpr float minimum_camera_inset = 16.0f;
	}

	PlayState::PlayState(Application* app,
		const std::array<InputSource, Arena::player_count>& sources) :
		app_(app), sources_(sources) {}

	PlayState::PlayState(Application* app, SmokeTest) :
		app_(app), smoke_test_(true) {}

	void PlayState::init()
	{
		this->white_ = this->app_->render_resources()->resolve_texture(white_texture_name);
		this->font_ = this->app_->render_resources()->resolve_sprite_font(font_name);
		this->scene_ = std::make_unique<Scene>(this->app_->thread_pool(), this->app_->partitioner());
		this->arena_ = this->scene_->add(std::make_unique<Arena>(this->white_));
		this->scene_->end_tick();
		this->app_->viewport_manager()->set_layout(ScreenLayout::two_player);
		this->rebuild_views();
	}

	void PlayState::update(float dt)
	{
		std::array<Vector2F, Arena::player_count> directions = smoke_directions;
		if (!this->smoke_test_)
		{
			const Keyboard& keyboard = *this->app_->keyboard();
			const Gamepads& pads = *this->app_->gamepads();
			bool quit = keyboard.pressed(Key::escape);
			for (const InputSource source : this->sources_)
			{
				const int slot = pad_slot(source);
				quit = quit || (slot >= 0 && pads.pressed(slot, GamepadButton::b));
			}
			if (quit)
			{
				this->app_->quit();
				return;
			}

			std::array<Vector2F, Gamepads::max_count> all_sticks;
			for (int slot = 0; slot < Gamepads::max_count; ++slot)
			{
				all_sticks[static_cast<std::size_t>(slot)] = pads.state(slot).left_stick;
			}
			directions = read_bound_directions(keyboard, all_sticks, this->sources_);
		}
		this->arena_->set_directions(directions);
		this->scene_->update(dt);
		this->scene_->end_tick();
		this->rebuild_views();
	}

	void PlayState::rebuild_views()
	{
		this->scene_->clear_views();
		for (size_t player = 0; player < Arena::player_count; ++player)
		{
			const Viewport viewport = this->app_->viewport_manager()->player_viewport(
				static_cast<int>(player));
			this->panes_[player] = viewport;
			// Reserve the title strip so a player near the world's top edge is
			// not covered by the HUD when its camera stops there.
			const float reserved = std::min(hud_height, std::max(0.0f, viewport.height - 1.0f));
			const Viewport world_view(viewport.x, viewport.y + reserved,
				viewport.width, viewport.height - reserved);
			const float inset_x = std::min(world_view.width * 0.5f,
				std::max(minimum_camera_inset, world_view.width * 0.25f));
			const float inset_y = std::min(world_view.height * 0.5f,
				std::max(minimum_camera_inset, world_view.height * 0.25f));
			const RectangleF dead_zone(inset_x, inset_y,
				world_view.width - 2.0f * inset_x, world_view.height - 2.0f * inset_y);
			this->cameras_[player].update(this->arena_->position(player),
				world_view.size(), dead_zone, this->arena_->bounds());
			this->scene_->add_view(world_view, this->cameras_[player].camera());
		}
	}

	void PlayState::draw(Renderer& renderer) const
	{
		this->scene_->draw(renderer, [this](int view_index, DrawList& list)
		{
			const size_t player = static_cast<size_t>(view_index);
			const Viewport& viewport = this->panes_[player];
			const Viewport& world_view = this->scene_->view(view_index).viewport;
			const float reserved = world_view.y - viewport.y;
			list.set_viewport(viewport);
			list.set_camera(Camera::default_camera);
			list.draw_sprite(this->white_, RectangleI(0, 0, 1, 1),
				RectangleF(0.0f, 0.0f, viewport.width, reserved), Colour(12, 18, 27),
				0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
			// Very short panes retain the world strip rather than painting
			// instructions over it when the title cannot fit.
			if (reserved >= hud_height)
			{
				// The smoke test joins no one, so its titles are fixed text, which
				// its capture shows; a match names the source each player joined
				// with.
				std::wstring title = player == 0 ?
					L"PLAYER 1  |  WASD / pad slot 0" : L"PLAYER 2  |  Arrows / pad slot 1";
				if (!this->smoke_test_)
				{
					title = L"PLAYER " + std::to_wstring(player + 1) + L"  |  " +
						source_name(this->sources_[player]);
				}
				list.draw_text(this->font_, title, Vector2F(18.0f, 12.0f),
					Arena::player_colour(player), 1.0f, 0.0f, Vector2F::zero, 0.0f);
				list.draw_text(this->font_, L"One world, two cameras. Meet at the purple cross.  Esc / B: quit",
					Vector2F(18.0f, 38.0f), Colour::white, 1.0f, 0.0f, Vector2F::zero, 0.0f);
			}
			if (world_view.height > 4.0f)
			{
				list.draw_sprite(this->white_, RectangleI(0, 0, 1, 1),
					RectangleF(0.0f, viewport.height - 2.0f, viewport.width, 2.0f),
					Arena::player_colour(player), 0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
			}
		});
	}

	void PlayState::verify_smoke_test() const
	{
		const Vector2F first = this->arena_->position(0);
		const Vector2F second = this->arena_->position(1);
		if (std::abs(first.x - 1200.0f) > 0.01f || first.y != 540.0f ||
			second.x != 960.0f || std::abs(second.y - 60.0f) > 0.01f ||
			this->scene_->view_count() != 2)
		{
			throw std::runtime_error("Smoke test: scripted players or views diverged");
		}
	}
}
