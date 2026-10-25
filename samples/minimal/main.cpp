// The smallest thing that is still a game on this engine, and the file a new
// project starts from: copy samples/minimal/, rename the target, and own
// everything inside it.
//
// Everything here is a decision the engine cannot make for you - what the
// window is called, how big it is, where your content lives, and which of your
// states runs first. Everything it does not say is the engine's, and lives in
// Application (engine/app/application.h).

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "samples/minimal/states/hello_state.h"

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <memory>
#include <string_view>
using namespace labrador;

namespace
{
	// Whether the command line asks for the smoke test, whatever spaces the
	// program that started the game put around it.
	bool asks_for_smoke_test(std::wstring_view arguments)
	{
		const std::size_t first = arguments.find_first_not_of(L" \t");
		const std::size_t last = arguments.find_last_not_of(L" \t");
		return first != std::wstring_view::npos &&
			arguments.substr(first, last - first + 1) == L"--smoke-test";
	}

	// How many frames --smoke-test draws before it quits: half a second at
	// 60 Hz, which is enough for the first state to have initialised, stepped
	// and drawn several times over.
	constexpr std::uint64_t smoke_test_frames = 30;

	// --smoke-test: the game's first state, run as it is in a hidden window,
	// and the game quit once the loop has drawn smoke_test_frames frames. It
	// is a start-up check rather than a test of what the state does - the
	// window, the device, the content and the first frames, which is what an
	// engine upgrade that moved something would break first - and ctest runs
	// it (CMakeLists.txt, MinimalSmokeTests).
	//
	// It wraps the state rather than adding a flag to it, so HelloState has no
	// test code in it and a copy wraps its own first state the same way.
	class SmokeTest : public State
	{
	public:
		SmokeTest(Application* app, std::unique_ptr<State> game) :
			app_(app), game_(std::move(game))
		{
		}

		void init() override
		{
			// The game's state reaches the stack through this one's context,
			// so anything it pushes lands on the same stack.
			this->game_->set_context(this->context());
			this->game_->init();
		}

		void update(float dt) override
		{
			this->game_->update(dt);

			// Once: the tick that quits still runs every step the clock owes.
			if (!this->quit_ &&
				this->app_->frame_timings().frame >= smoke_test_frames)
			{
				this->app_->quit();
				this->quit_ = true;
			}
		}

		void draw(Renderer& renderer) const override
		{
			this->game_->draw(renderer);
		}

		bool covers_screen() const override
		{
			return this->game_->covers_screen();
		}
		void on_suspend() override { this->game_->on_suspend(); }
		void on_resume() override { this->game_->on_resume(); }
		void on_activated() override { this->game_->on_activated(); }
		void on_deactivated() override { this->game_->on_deactivated(); }

	private:
		Application* app_ = nullptr;
		std::unique_ptr<State> game_ = nullptr;
		bool quit_ = false;
	};
}

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE,
	_In_ LPWSTR command_line, _In_ int)
{
	const bool smoke_test = asks_for_smoke_test(command_line);
	try
	{
		ApplicationOptions options;
		options.window_class_name = L"MinimalSampleWindowClass";
		// ASCII only: this file gets copied into new projects, and a wide
		// literal's encoding depends on how the compiler was told to read the
		// source.
		options.window_title = L"Labrador - minimal sample";
		options.resolution = ScreenResolution::s_1280_720;

		// A smoke test gets the whole shell - window, device, services - with
		// nothing on screen and no focus taken.
		options.visible = !smoke_test;

		// One pane, so one view's worth of recording state. The default is four
		// - four-player split-screen, the widest layout the engine has a client
		// for - and every view above what a frame draws is a deferred context
		// and a dynamic vertex buffer created at startup and never used.
		// A game that fans out says so here; this one does not.
		options.view_capacity = 1;

		Application app(std::move(options));
		app.initialize();

		// Everything this sample draws, named in content/manifest.json. A game
		// with its own kinds of asset - levels, dialogue, whatever it has -
		// teaches them to app.resource_loader() before this line.
		//
		// Beside the executable, wherever the executable was started from:
		// a relative path here is relative to the game, not to the working
		// directory (Application::load_manifest), and the build copies the
		// content there.
		app.load_manifest("./manifest.json");

		std::unique_ptr<State> first_state = std::make_unique<HelloState>(&app);
		if (smoke_test)
		{
			first_state = std::make_unique<SmokeTest>(&app, std::move(first_state));
		}
		return app.run(std::move(first_state));
	}
	catch (const std::exception& e)
	{
		// A broken contract stops the program dead with the reason on screen,
		// never a silent abort (PHILOSOPHY T6). A smoke test has nobody to
		// close a message box, so it writes the reason and exits non-zero.
		fprintf(stderr, "startup failure: %s\n", e.what());
		if (!smoke_test)
		{
			MessageBoxA(nullptr, e.what(), "Sample - startup failure",
				MB_OK | MB_ICONERROR);
		}
		return 1;
	}
}
