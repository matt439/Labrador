#include <doctest/doctest.h>

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/math/vector2f.h"
#include "engine/render/renderer.h"
#include "engine/render/resolution_manager.h"

#include <Windows.h>

#include <memory>
#include <stdexcept>

namespace
{
	using namespace labrador;

	struct RevealObservation
	{
		int draws = 0;
		bool first_draw_visible = false;
		bool completed_frame_visible = false;
	};

	class RevealState : public State
	{
	public:
		RevealState(Application* app, RevealObservation* observation,
			bool fail_draw = false, bool quit_in_init = false) :
			app_(app), observation_(observation), fail_draw_(fail_draw),
			quit_in_init_(quit_in_init)
		{
		}

		void init() override
		{
			CHECK_FALSE(IsWindowVisible(static_cast<HWND>(this->app_->window())));
			if (this->quit_in_init_)
			{
				this->app_->quit();
			}
		}

		void update(float) override
		{
			if (this->app_->frame_timings().frame > 0 && this->app_->window())
			{
				this->observation_->completed_frame_visible =
					IsWindowVisible(static_cast<HWND>(this->app_->window())) != FALSE;
				this->app_->quit();
			}
		}

		void draw(Renderer&) const override
		{
			if (this->observation_->draws++ == 0)
			{
				this->observation_->first_draw_visible =
					IsWindowVisible(static_cast<HWND>(this->app_->window())) != FALSE;
			}
			if (this->fail_draw_)
			{
				throw std::runtime_error("first draw failed");
			}
		}

	private:
		Application* app_;
		RevealObservation* observation_;
		bool fail_draw_;
		bool quit_in_init_;
	};

	ApplicationOptions reveal_options()
	{
		ApplicationOptions options;
		options.window_class_name = L"ApplicationRevealTestWindow";
		options.client_size = Vector2I(400, 240);
		options.view_capacity = 1;
		options.max_threads = 1;
		options.target_fps = 240;
		return options;
	}
}

TEST_CASE("Application reveals only a completed frame and preserves explicit hidden startup")
{
	ApplicationOptions options = reveal_options();
	SUBCASE("visible windowed") {}
	SUBCASE("visible fullscreen") { options.fullscreen = true; }
	SUBCASE("hidden windowed") { options.visible = false; }
	SUBCASE("hidden fullscreen")
	{
		options.visible = false;
		options.fullscreen = true;
	}
	Application app(options);
	app.initialize();
	const HWND window = static_cast<HWND>(app.window());
	REQUIRE(window != nullptr);
	CHECK_FALSE(IsWindowVisible(window));
	CHECK(GetClassLongPtrW(window, GCLP_HBRBACKGROUND) == 0);

	RECT client = {};
	REQUIRE(GetClientRect(window, &client));
	const Vector2F size(static_cast<float>(client.right - client.left),
		static_cast<float>(client.bottom - client.top));
	CHECK(app.renderer()->back_buffer_size() == size);
	CHECK(app.resolution_manager()->resolution_vec() == size);
	if (options.visible && options.fullscreen)
	{
		MONITORINFO monitor = {};
		monitor.cbSize = sizeof(monitor);
		REQUIRE(GetMonitorInfoW(MonitorFromWindow(window,
			MONITOR_DEFAULTTONEAREST), &monitor));
		CHECK(size == Vector2F(static_cast<float>(monitor.rcMonitor.right -
			monitor.rcMonitor.left), static_cast<float>(monitor.rcMonitor.bottom -
			monitor.rcMonitor.top)));
	}

	RevealObservation observation;
	CHECK(app.run(std::make_unique<RevealState>(&app, &observation)) == 0);
	CHECK(observation.draws > 0);
	CHECK_FALSE(observation.first_draw_visible);
	CHECK(observation.completed_frame_visible == options.visible);
	CHECK(app.window() == nullptr);
}

TEST_CASE("a failed first draw and quit during init never reveal an Application")
{
	Application app(reveal_options());
	app.initialize();
	const HWND window = static_cast<HWND>(app.window());
	RevealObservation observation;
	SUBCASE("failed first draw")
	{
		CHECK_THROWS_WITH_AS(app.run(std::make_unique<RevealState>(
			&app, &observation, true)), "first draw failed", std::runtime_error);
		CHECK_FALSE(IsWindowVisible(window));
		CHECK(app.frame_timings().frame == 0);
	}
	SUBCASE("quit before the first frame")
	{
		CHECK(app.run(std::make_unique<RevealState>(
			&app, &observation, false, true)) == 0);
		CHECK(observation.draws == 0);
		CHECK_FALSE(IsWindow(window));
	}
	CHECK_FALSE(observation.first_draw_visible);
}

TEST_CASE("fullscreen changes before the first frame do not reveal an Application")
{
	ApplicationOptions options = reveal_options();
	bool leave_fullscreen = false;
	SUBCASE("enter before reveal") {}
	SUBCASE("enter then leave before reveal") { leave_fullscreen = true; }
	SUBCASE("hidden through both changes")
	{
		options.visible = false;
		leave_fullscreen = true;
	}
	Application app(options);
	app.initialize();
	const HWND window = static_cast<HWND>(app.window());
	app.set_fullscreen(true);
	CHECK_FALSE(IsWindowVisible(window));
	if (leave_fullscreen)
	{
		app.set_fullscreen(false);
		CHECK_FALSE(IsWindowVisible(window));
	}
	RevealObservation observation;
	CHECK(app.run(std::make_unique<RevealState>(&app, &observation)) == 0);
	CHECK_FALSE(observation.first_draw_visible);
	CHECK(observation.completed_frame_visible == options.visible);
}
