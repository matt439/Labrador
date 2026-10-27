#include "samples/collision/collision_state.h"

#include "engine/input/keyboard.h"
#include "engine/render/resource_factory.h"

#include <cmath>
#include <stdexcept>
#include <string>

using namespace labrador;
using namespace labrador;

namespace collisiondemo
{
	namespace
	{
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

	CollisionState::CollisionState(Application* app, bool smoke_test) :
		app_(app), smoke_test_(smoke_test)
	{
	}

	void CollisionState::init()
	{
		RenderResources* resources = this->app_->render_resources();
		this->font_ = resources->resolve_sprite_font("jetbrains_mono_bold_16");
		this->arena_ = std::make_unique<Arena>(resources->resolve_texture(white_texture_name));
		this->refresh_view();
	}

	void CollisionState::update(float dt)
	{
		const Keyboard& keyboard = *this->app_->keyboard();
		const Gamepads& pads = *this->app_->gamepads();
		if (keyboard.pressed(Key::escape) || pads.pressed(0, GamepadButton::b))
		{
			this->app_->quit();
			return;
		}
		if (keyboard.pressed(Key::r) || pads.pressed(0, GamepadButton::y))
		{
			this->arena_->reset();
		}
		Vector2F direction = keyboard_direction(keyboard) +
			apply_deadzone(pads.state(0).left_stick, 0.2f);
		if (this->smoke_test_)
		{
			if (this->ticks_ < 120) { direction = Vector2F(1.0f, 0.0f); }
			else if (this->ticks_ < 180) { direction = Vector2F(0.0f, 1.0f); }
			else if (this->ticks_ < 280) { direction = Vector2F(1.0f, 0.0f); }
			else { direction = Vector2F(0.0f, -1.0f); }
		}
		this->arena_->step(direction, dt);
		this->refresh_view();
		this->status_ = L"Contacts this tick: " +
			std::to_wstring(this->arena_->contact_count()) + L"    Trigger entries: " +
			std::to_wstring(this->arena_->trigger_entries());
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
			std::abs(player.x - 968.0f) > 0.01f || std::abs(player.y - 420.0f) > 0.01f)
		{
			throw std::runtime_error("Collision smoke: wall/trigger route failed");
		}
	}

	void CollisionState::draw(Renderer& renderer) const
	{
		this->arena_->scene().draw(renderer, [this](int, DrawList& draw_list)
		{
			// The overlay keeps the world's camera so its labels resize with
			// the arena, in the same fitted 1280x720 coordinates.
			const auto line = [this, &draw_list](const std::wstring& text,
				float x, float y, const Colour& tint)
			{
				draw_list.draw_text(this->font_, text, Vector2F(x, y), tint,
					1.0f, 0.0f, Vector2F::zero, 0.0f);
			};
			line(L"COLLISION / move, resolve, observe", 40.0f, 30.0f, Colour::white);
			line(L"WASD / arrows / left stick: move    R / Y: reset    Esc / B: quit",
				40.0f, 60.0f, Colour(180, 195, 215));
			line(this->status_, 40.0f, 94.0f, Colour::white);
			line(L"MASK = 0", 348.0f, 276.0f, Colour(190, 150, 225));
			line(L"SOLID", 600.0f, 246.0f, Colour(175, 195, 215));
			line(L"TRIGGER", 914.0f, 276.0f, Colour(90, 235, 145));
			line(L"Blue: player   Gold: wall contact   Green: inside trigger",
				40.0f, 682.0f, Colour(180, 195, 215));
		});
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
