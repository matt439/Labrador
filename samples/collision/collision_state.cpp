#include "samples/collision/collision_state.h"

#include "engine/input/gamepad.h"
#include "engine/input/gamepads.h"
#include "engine/input/keyboard.h"
#include "engine/render/resource_factory.h"
#include "samples/collision/pads.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>

using namespace labrador;

namespace collision
{
	namespace
	{
		// How far a stick has to move before it counts, applied once per slot
		// by the code that knows what the stick is for (engine/input/gamepad.h).
		constexpr float stick_deadzone = 0.2f;

		// The smoke route, one leg per row: the direction held until the tick
		// the leg ends on. Right into the wall's face, down past its foot,
		// right into the trigger, then up inside it, ending at smoke_route_end.
		struct RouteLeg
		{
			int end_tick;
			float x;
			float y;
		};
		constexpr RouteLeg smoke_route[] = {
			{ 120, 1.0f, 0.0f },
			{ 180, 0.0f, 1.0f },
			{ 280, 1.0f, 0.0f },
			{ smoke_route_ticks, 0.0f, -1.0f },
		};

		// Where the player's top-left corner is after smoke_route_ticks steps.
		constexpr Vector2F smoke_route_end{ 968.0f, 420.0f };

		Vector2F keyboard_direction(const Keyboard& keyboard)
		{
			Vector2F direction = Vector2F::zero;
			if (keyboard.held(Key::a) || keyboard.held(Key::left)) { direction.x -= 1.0f; }
			if (keyboard.held(Key::d) || keyboard.held(Key::right)) { direction.x += 1.0f; }
			if (keyboard.held(Key::w) || keyboard.held(Key::up)) { direction.y -= 1.0f; }
			if (keyboard.held(Key::s) || keyboard.held(Key::down)) { direction.y += 1.0f; }
			return direction;
		}
	}

	CollisionState::CollisionState(Application* app, bool smoke_test,
		bool show_outlines) :
		app_(app), smoke_test_(smoke_test), show_outlines_(show_outlines)
	{
	}

	void CollisionState::init()
	{
		RenderResources* resources = this->app_->render_resources();
		this->font_ = resources->resolve_sprite_font("jetbrains_mono_bold_16");
		const TextureHandle white = resources->resolve_texture(white_texture_name);
		this->arena_ = std::make_unique<Arena>(white);
		this->outlines_ = components::ShapeOutline(white);
		this->refresh_view();
	}

	void CollisionState::update(float dt)
	{
		const Keyboard& keyboard = *this->app_->keyboard();
		const Gamepads& pads = *this->app_->gamepads();
		std::array<PadReading, Gamepads::max_count> readings;
		for (int slot = 0; slot < Gamepads::max_count; ++slot)
		{
			PadReading& reading = readings[static_cast<std::size_t>(slot)];
			reading.stick = apply_deadzone(pads.state(slot).left_stick, stick_deadzone);
			reading.reset_pressed = pads.pressed(slot, GamepadButton::y);
			reading.quit_pressed = pads.pressed(slot, GamepadButton::b);
		}
		const PadsInput pad_input = combine_pads(readings);
		if (keyboard.pressed(Key::escape) || pad_input.quit_requested)
		{
			this->app_->quit();
			return;
		}
		if (keyboard.pressed(Key::r) || pad_input.reset_requested)
		{
			this->arena_->reset();
		}
		if (keyboard.pressed(Key::o))
		{
			this->show_outlines_ = !this->show_outlines_;
		}
		Vector2F direction = keyboard_direction(keyboard) + pad_input.stick;
		if (this->smoke_test_)
		{
			direction = Vector2F::zero;
			for (const RouteLeg& leg : smoke_route)
			{
				if (this->ticks_ < leg.end_tick)
				{
					direction = Vector2F(leg.x, leg.y);
					break;
				}
			}
		}
		this->arena_->step(direction, dt);
		this->refresh_view();
		this->status_ = L"Contacts this tick: " +
			std::to_wstring(this->arena_->contact_count()) + L"    Trigger entries: " +
			std::to_wstring(this->arena_->trigger_entries()) + L"    Entry markers: " +
			std::to_wstring(this->arena_->entry_marker_count());
		if (this->smoke_test_)
		{
			++this->ticks_;
		}
	}

	void CollisionState::verify_smoke_test() const
	{
		const RectangleF player = this->arena_->player().bounds();
		if (!this->arena_->hit_wall() || this->arena_->trigger_entries() != 1 ||
			!this->arena_->player().touching_trigger() ||
			this->arena_->markers_spawned() != 1 || this->arena_->entry_marker_count() != 0 ||
			std::abs(player.x - smoke_route_end.x) > 0.01f ||
			std::abs(player.y - smoke_route_end.y) > 0.01f)
		{
			throw std::runtime_error("Smoke test: wall/trigger route or marker lifetime failed");
		}
	}

	void CollisionState::draw(Renderer& renderer) const
	{
		this->arena_->scene().draw(renderer, [this](int, DrawList& draw_list)
		{
			if (this->show_outlines_)
			{
				this->draw_outlines(draw_list);
			}
			// The overlay keeps the world's camera so its labels resize with
			// the arena, in the same fitted 1280x720 coordinates.
			const auto line = [this, &draw_list](const std::wstring& text,
				float x, float y, const Colour& tint)
			{
				draw_list.draw_text(this->font_, text, Vector2F(x, y), tint,
					1.0f, 0.0f, Vector2F::zero, 0.0f);
			};
			line(L"COLLISION / move, resolve, observe", 40.0f, 30.0f, Colour::white);
			line(L"WASD / arrows / left stick: move    R / Y: reset    O: outlines    Esc / B: quit",
				40.0f, 60.0f, Colour(180, 195, 215));
			line(this->status_, 40.0f, 94.0f, Colour::white);
			line(L"MASK = 0", 348.0f, 276.0f, Colour(190, 150, 225));
			line(L"SOLID", 600.0f, 246.0f, Colour(175, 195, 215));
			line(L"TRIGGER", 914.0f, 276.0f, Colour(90, 235, 145));
			line(L"Blue: player   Gold: wall contact   Green: inside trigger",
				40.0f, 682.0f, Colour(180, 195, 215));
		});
	}

	void CollisionState::draw_outlines(DrawList& draw_list) const
	{
		// The game chooses every registered collider, including mask-zero
		// decoration. Drawing reads the same shapes the sweep sees.
		for (const CollisionObject* object : this->arena_->scene().collision_objects())
		{
			if (const Shape* shape = object->shape())
			{
				this->outlines_.draw(draw_list, *shape, 2.0f, Colour::yellow, 32);
			}
		}
	}

	void CollisionState::refresh_view()
	{
		const Vector2F resolution = this->app_->resolution_manager()->resolution_vec();
		const Viewport viewport(RectangleF(Vector2F::zero, resolution));
		const Camera camera = Camera::frame(RectangleF(0.0f, 0.0f, 1280.0f, 720.0f),
			viewport);
		this->arena_->scene().clear_views();
		this->arena_->scene().add_view(viewport, camera);
	}
}
