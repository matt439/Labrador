#include "samples/local_multiplayer/play_state.h"

#include "samples/local_multiplayer/controls.h"

#include <cmath>
#include <stdexcept>
#include <string>

using namespace labrador;
using namespace mattmath;

namespace multiplayer
{
	PlayState::PlayState(Application* app, bool smoke_test) :
		app_(app), smoke_test_(smoke_test) {}

	void PlayState::init()
	{
		this->white_ = this->app_->render_resources()->resolve_texture("white");
		this->font_ = this->app_->render_resources()->resolve_sprite_font("courier_new_bold_16");
		this->scene_ = std::make_unique<Scene>(this->app_->thread_pool(), this->app_->partitioner());
		this->arena_ = this->scene_->add(std::make_unique<Arena>(this->white_));
		this->scene_->end_tick();
		this->app_->viewport_manager()->set_layout(ScreenLayout::two_player);
		this->rebuild_views();
	}

	void PlayState::update(float dt)
	{
		const Keyboard& keyboard = *this->app_->keyboard();
		const Gamepads& pads = *this->app_->gamepads();
		if (!this->smoke_test_ && (keyboard.pressed(Key::escape) ||
			pads.pressed(0, GamepadButton::b) || pads.pressed(1, GamepadButton::b)))
		{
			this->app_->quit();
			return;
		}

		const std::array<Vector2F, Arena::player_count> sticks = {
			pads.state(0).left_stick, pads.state(1).left_stick
		};
		std::array<Vector2F, Arena::player_count> directions = read_directions(keyboard, sticks);
		if (this->smoke_test_)
		{
			directions = { Vector2F(1.0f, 0.0f), Vector2F(0.0f, -1.0f) };
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
			this->scene_->add_view(viewport, this->arena_->camera(player, viewport));
		}
	}

	void PlayState::draw(Renderer& renderer) const
	{
		this->scene_->draw(renderer, [this](int view_index, DrawList& list)
		{
			const size_t player = static_cast<size_t>(view_index);
			const Viewport& viewport = this->scene_->view(view_index).viewport;
			list.set_camera(Camera::DEFAULT_CAMERA);
			list.draw_sprite(this->white_, RectangleI(0, 0, 1, 1),
				RectangleF(0.0f, 0.0f, viewport.width, 68.0f), Colour(12, 18, 27),
				0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);
			const std::wstring title = player == 0 ?
				L"PLAYER 1  |  WASD / pad slot 0" : L"PLAYER 2  |  Arrows / pad slot 1";
			list.draw_text(this->font_, title, Vector2F(18.0f, 12.0f),
				Arena::player_colour(player), 1.0f, 0.0f, Vector2F::ZERO, 0.0f);
			list.draw_text(this->font_, L"One world, two cameras. Meet at the purple cross.  Esc / B: quit",
				Vector2F(18.0f, 38.0f), Colour::white, 1.0f, 0.0f, Vector2F::ZERO, 0.0f);
			list.draw_sprite(this->white_, RectangleI(0, 0, 1, 1),
				RectangleF(0.0f, viewport.height - 2.0f, viewport.width, 2.0f),
				Arena::player_colour(player), 0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);
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
			throw std::runtime_error("LocalMultiplayerSample: scripted players or views diverged");
		}
	}
}
