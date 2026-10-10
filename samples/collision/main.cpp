#include "samples/collision/arena.h"

#include "engine/app/application.h"
#include "engine/input/keyboard.h"
#ifdef COLLISION_SAMPLE_CAPTURE
#include "tools/linesweeper_capture/png_file.h"
#endif

#include <Windows.h>

#include <cstdio>
#include <cmath>
#include <exception>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace mattmath;
using namespace labrador;

namespace
{
	std::wstring_view trim_arguments(std::wstring_view arguments)
	{
		const std::size_t first = arguments.find_first_not_of(L" \t\r\n");
		if (first == std::wstring_view::npos)
		{
			return {};
		}
		const std::size_t last = arguments.find_last_not_of(L" \t\r\n");
		return arguments.substr(first, last - first + 1);
	}

	Vector2F keyboard_direction(const Keyboard& keyboard)
	{
		Vector2F direction = Vector2F::ZERO;
		if (keyboard.held(Key::a) || keyboard.held(Key::left)) { direction.x -= 1.0f; }
		if (keyboard.held(Key::d) || keyboard.held(Key::right)) { direction.x += 1.0f; }
		if (keyboard.held(Key::w) || keyboard.held(Key::up)) { direction.y -= 1.0f; }
		if (keyboard.held(Key::s) || keyboard.held(Key::down)) { direction.y += 1.0f; }
		return direction;
	}

	class CollisionState final : public State
	{
	public:
		CollisionState(Application* app, bool smoke_test) :
			app_(app), smoke_test_(smoke_test)
		{
		}

		void init() override
		{
			RenderResources* resources = this->app_->render_resources();
			this->font_ = resources->resolve_sprite_font("courier_new_bold_16");
			this->arena_ = std::make_unique<collisiondemo::Arena>(
				resources->resolve_texture("white"));
			this->refresh_view();
		}

		void update(float dt) override
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

		void verify_smoke_test() const
		{
			const RectangleF player = this->arena_->player().bounds();
			if (!this->arena_->hit_wall() || this->arena_->trigger_entries() != 1 ||
				!this->arena_->player().touching_trigger() ||
				std::abs(player.x - 968.0f) > 0.01f || std::abs(player.y - 420.0f) > 0.01f)
			{
				throw std::runtime_error("Collision smoke: wall/trigger route failed");
			}
		}

		void draw(Renderer& renderer) const override
		{
			this->arena_->scene().draw(renderer, [this](int, DrawList& draw_list)
			{
				// The overlay keeps the world's camera so its labels resize with
				// the arena, in the same fitted 1280x720 coordinates.
				const auto line = [this, &draw_list](const std::wstring& text,
					float x, float y, const Colour& tint)
				{
					draw_list.draw_text(this->font_, text, Vector2F(x, y), tint,
						1.0f, 0.0f, Vector2F::ZERO, 0.0f);
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

	private:
		void refresh_view()
		{
			const Vector2F resolution = this->app_->resolution_manager()->resolution_vec();
			const Viewport viewport(RectangleF(Vector2F::ZERO, resolution));
			const Camera camera = Camera::frame(RectangleF(0.0f, 0.0f, 1280.0f, 720.0f),
				viewport);
			this->arena_->scene().clear_views();
			this->arena_->scene().add_view(viewport, camera);
		}

		// The application owns this state during normal play and outlives it.
		Application* app_;
		bool smoke_test_;
		std::unique_ptr<collisiondemo::Arena> arena_;
		FontHandle font_;
		std::wstring status_ = L"Contacts this tick: 0    Trigger entries: 0";
		int ticks_ = 0;
	};
}

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE,
	_In_ LPWSTR command_line, _In_ int)
{
	const std::wstring_view arguments = trim_arguments(command_line);
	const bool smoke_test = arguments.starts_with(L"--smoke-test");
	try
	{
		std::filesystem::path capture_path;
		constexpr std::wstring_view capture_prefix = L"--smoke-test --capture ";
		if (arguments.starts_with(capture_prefix))
		{
			std::wstring_view path = arguments.substr(capture_prefix.size());
			if (path.size() >= 2 && path.front() == L'"' && path.back() == L'"')
			{
				path = path.substr(1, path.size() - 2);
			}
			if (path.empty()) throw std::invalid_argument("Capture path must not be empty");
			capture_path = path;
#ifndef COLLISION_SAMPLE_CAPTURE
			throw std::invalid_argument("The null renderer cannot capture pixels");
#endif
		}
		else if (!arguments.empty() && arguments != L"--smoke-test")
		{
			throw std::invalid_argument("usage: CollisionSample [--smoke-test [--capture path.png]]");
		}
		ApplicationOptions options;
		options.window_class_name = L"CollisionSampleWindow";
		options.window_title = L"Labrador - collision sample";
		options.view_capacity = 1;
		options.max_threads = 1;
		options.visible = !smoke_test;
		Application app(std::move(options));
		app.initialize();
		app.load_manifest("./manifest.json");
		if (smoke_test)
		{
			CollisionState state(&app, true);
			state.init();
			for (int tick = 0; tick < 315; ++tick)
			{
				state.update(1.0f / 60.0f);
				app.renderer()->begin_frame();
				state.draw(*app.renderer());
				app.renderer()->submit();
#ifdef COLLISION_SAMPLE_CAPTURE
				if (tick == 314 && !capture_path.empty())
				{
					std::vector<unsigned char> pixels;
					app.renderer()->read_back_buffer(pixels);
					const Vector2F size = app.renderer()->back_buffer_size();
					capture::write_png(capture_path, static_cast<int>(size.x),
						static_cast<int>(size.y), pixels);
				}
#endif
				app.renderer()->end_frame();
			}
			state.verify_smoke_test();
			return 0;
		}
		return app.run(std::make_unique<CollisionState>(&app, smoke_test));
	}
	catch (const std::exception& error)
	{
		std::fprintf(stderr, "CollisionSample: %s\n", error.what());
		if (arguments.empty())
		{
			MessageBoxA(nullptr, error.what(), "CollisionSample failure", MB_OK | MB_ICONERROR);
		}
		return 1;
	}
}
