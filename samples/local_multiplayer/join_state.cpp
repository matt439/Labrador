#include "samples/local_multiplayer/join_state.h"

#include "engine/input/gamepads.h"
#include "engine/input/keyboard.h"
#include "engine/render/resource_factory.h"
#include "samples/local_multiplayer/controls.h"
#include "samples/local_multiplayer/play_state.h"

#include <array>
#include <memory>
#include <string>

using namespace labrador;

namespace multiplayer
{
	JoinState::JoinState(Application* app) :
		app_(app), focus_(static_cast<int>(Arena::player_count))
	{
	}

	const Lobby& JoinState::lobby() const
	{
		return this->lobby_;
	}

	void JoinState::init()
	{
		RenderResources* resources = this->app_->render_resources();
		this->white_ = resources->resolve_texture(white_texture_name);
		this->font_ = resources->resolve_sprite_font("jetbrains_mono_bold_16");
		this->scene_ = std::make_unique<Scene>(nullptr, nullptr);
		this->rows_[0] = this->scene_->add(std::make_unique<UiText>(
			"ready", L"READY", "jetbrains_mono_bold_16", Vector2F(78.0f, 118.0f), resources));
		this->rows_[1] = this->scene_->add(std::make_unique<UiText>(
			"leave", L"LEAVE", "jetbrains_mono_bold_16", Vector2F(78.0f, 164.0f), resources));
		this->focus_.add(this->rows_[0], [this]()
		{
			this->lobby_ = choose_lobby(this->lobby_, this->activating_player_, JoinChoice::ready);
		});
		this->focus_.add(this->rows_[1], [this]()
		{
			this->lobby_ = choose_lobby(this->lobby_, this->activating_player_, JoinChoice::leave);
		});
		this->scene_->end_tick();
		this->app_->viewport_manager()->set_layout(ScreenLayout::two_player);
		this->rebuild_views();
	}

	Direction JoinState::held_direction(InputSource source) const
	{
		const Keyboard& keys = *this->app_->keyboard();
		if (source == InputSource::keyboard_left)
		{
			if (keys.held(Key::w)) return Direction::up;
			if (keys.held(Key::s)) return Direction::down;
			if (keys.held(Key::a)) return Direction::left;
			if (keys.held(Key::d)) return Direction::right;
		}
		else if (source == InputSource::keyboard_right)
		{
			if (keys.held(Key::up)) return Direction::up;
			if (keys.held(Key::down)) return Direction::down;
			if (keys.held(Key::left)) return Direction::left;
			if (keys.held(Key::right)) return Direction::right;
		}
		else if (const int slot = pad_slot(source); slot >= 0)
		{
			return pad_direction(this->app_->gamepads()->state(slot));
		}
		return Direction::none;
	}

	bool JoinState::confirm_pressed(InputSource source) const
	{
		if (source == InputSource::keyboard_left) return this->app_->keyboard()->pressed(Key::f);
		if (source == InputSource::keyboard_right) return this->app_->keyboard()->pressed(Key::enter);
		const int slot = pad_slot(source);
		return slot >= 0 && this->app_->gamepads()->pressed(slot, GamepadButton::a);
	}

	bool JoinState::leave_pressed(InputSource source) const
	{
		if (source == InputSource::keyboard_left) return this->app_->keyboard()->pressed(Key::g);
		if (source == InputSource::keyboard_right) return this->app_->keyboard()->pressed(Key::backspace);
		const int slot = pad_slot(source);
		return slot >= 0 && this->app_->gamepads()->pressed(slot, GamepadButton::b);
	}

	void JoinState::update(float dt)
	{
		const Keyboard& keyboard = *this->app_->keyboard();
		const Gamepads& pads = *this->app_->gamepads();
		if (keyboard.pressed(Key::escape))
		{
			this->app_->quit();
			return;
		}

		static_assert(input_source_count == static_cast<std::size_t>(Gamepads::max_count) + 2);
		std::array<JoinInput, input_source_count> input = {};
		input[0].connected = true;
		input[0].join_pressed = keyboard.pressed(Key::f);
		input[1].connected = true;
		input[1].join_pressed = keyboard.pressed(Key::enter);
		for (int slot = 0; slot < Gamepads::max_count; ++slot)
		{
			JoinInput& source = input[static_cast<std::size_t>(slot) + 2];
			source.connected = pads.connected(slot);
			source.just_connected = pads.just_connected(slot);
			source.just_disconnected = pads.just_disconnected(slot);
			source.join_pressed = pads.pressed(slot, GamepadButton::start);
			if (source.just_connected)
			{
				this->connection_notice_ = L"Pad slot " + std::to_wstring(slot) +
					L" connected. Release Start, then press it to join.";
			}
			else if (source.just_disconnected)
			{
				this->connection_notice_ = L"Pad slot " + std::to_wstring(slot) +
					L" disconnected. Its player can join again with any free source.";
			}
		}

		const Lobby before = this->lobby_;
		this->lobby_ = update_lobby(this->lobby_, input);
		for (std::size_t player = 0; player < Arena::player_count; ++player)
		{
			const InputSource source = this->lobby_.players[player].source;
			if (source != before.players[player].source)
			{
				this->focus_.set_focused(static_cast<int>(player), this->rows_[0]);
				this->repeats_[player].start_held(this->held_direction(source));
				// The join press cannot also choose READY on the same update.
				continue;
			}
			if (source == InputSource::none) continue;
			if (this->leave_pressed(source))
			{
				this->lobby_ = choose_lobby(this->lobby_, player, JoinChoice::leave);
				continue;
			}
			const Direction move = this->repeats_[player].update(this->held_direction(source), dt);
			if (move == Direction::up || move == Direction::down)
			{
				this->focus_.move(static_cast<int>(player), move);
			}
			if (this->confirm_pressed(source))
			{
				this->activating_player_ = player;
				this->focus_.activate(static_cast<int>(player));
			}
		}

		if (all_ready(this->lobby_))
		{
			const std::array<InputSource, Arena::player_count> sources = {
				this->lobby_.players[0].source, this->lobby_.players[1].source
			};
			this->context()->transition_to(std::make_unique<PlayState>(this->app_, sources));
			return;
		}
		this->scene_->update(dt);
		this->scene_->end_tick();
		this->rebuild_views();
	}

	void JoinState::rebuild_views()
	{
		this->scene_->clear_views();
		for (std::size_t player = 0; player < Arena::player_count; ++player)
		{
			const Viewport viewport = this->app_->viewport_manager()->player_viewport(
				static_cast<int>(player));
			// Fit the shared authored menu to each pane, including the minimum
			// window size. The overlay uses this same camera as the widgets.
			const Camera camera = Camera::frame(RectangleF(0.0f, 0.0f, 1280.0f, 360.0f), viewport);
			this->scene_->add_view(viewport, camera);
		}
	}

	void JoinState::draw(Renderer& renderer) const
	{
		this->scene_->draw(renderer, [this](int view_index, DrawList& list)
		{
			const std::size_t player = static_cast<std::size_t>(view_index);
			const JoinedPlayer& joined = this->lobby_.players[player];
			const Colour colour = Arena::player_colour(player);
			const std::wstring title = L"PLAYER " + std::to_wstring(player + 1) + L"  |  " +
				source_name(joined.source) + (joined.ready ? L"  |  READY" : L"");
			list.draw_text(this->font_, title, Vector2F(18.0f, 18.0f),
				colour, 1.0f, 0.0f, Vector2F::zero, 0.0f);
			list.draw_text(this->font_, L"Join: F for WASD, Enter for arrows, Start on any pad.",
				Vector2F(18.0f, 50.0f), Colour::white, 1.0f, 0.0f, Vector2F::zero, 0.0f);
			list.draw_text(this->font_, L"Move your cursor: W/S / Up/Down / pad up/down. Select: F / Enter / A.",
				Vector2F(18.0f, 220.0f), Colour::white, 1.0f, 0.0f, Vector2F::zero, 0.0f);
			list.draw_text(this->font_, L"Leave: G / Backspace / B. Both players choose READY to begin. Escape quits.",
				Vector2F(18.0f, 246.0f), Colour::white, 1.0f, 0.0f, Vector2F::zero, 0.0f);
			list.draw_text(this->font_, this->connection_notice_, Vector2F(18.0f, 280.0f),
				Colour::gray, 1.0f, 0.0f, Vector2F::zero, 0.0f);
			if (joined.source != InputSource::none)
			{
				const RectangleF row = this->focus_.focused(view_index)->bounds();
				list.draw_sprite(this->white_, RectangleI(0, 0, 1, 1),
					RectangleF(row.x - 30.0f, row.y, 12.0f, row.height), colour,
					0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
			}
		});
	}
}
