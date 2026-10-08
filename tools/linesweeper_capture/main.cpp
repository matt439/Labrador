// LineSweeperCapture: exact, repeatable PNGs of the LineSweeper sample.
//
// It replays a script through the sample's own rules and draws the frames it
// names with the sample's own presentation on a real device, then reads each
// one back with Renderer::read_back_buffer. Nothing in it reads a clock, a key
// or a pad, so a frame is a function of the script, the build and the device.
//
// THE MATCH IS STEPPED HERE RATHER THAN DRIVEN THROUGH A WINDOW, and the
// reason is the README's: rules/ is a static library that links nothing, so a
// match is the bytes handed to tick() and the presentation only reads it.
// Driving the running sample with posted key messages depends on which frames
// the window had the keyboard, which is the dependency the rules layer was
// built to exclude (samples/linesweeper/README.md, Input is read as held,
// never as pressed).
//
// THE SCENE IS PlayState's, ASSEMBLED HERE, the way
// bench/linesweeper_frame_bench.cpp assembles it: the state owns its World
// privately and reads a keyboard, so the tool owns the World and registers
// the same objects in the same order. The two hint labels' text is the one
// part copied from play_state.cpp, as the bench copies it, and has to follow
// any change there. The pause screen is the sample's own PauseState, which
// only needs the shell's services to build and draw.
//
// tools/linesweeper_capture/README.md says how to run it and how to change
// the script.

#include "engine/app/application.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/label.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/viewport.h"
#include "engine/scene/scene.h"
#include "samples/linesweeper/presentation/board_view.h"
#include "samples/linesweeper/presentation/particles.h"
#include "samples/linesweeper/presentation/top_out_banner.h"
#include "samples/linesweeper/rules/world.h"
#include "samples/linesweeper/states/pause_state.h"
#include "tools/linesweeper_capture/capture_script.h"
#include "tools/linesweeper_capture/png_file.h"
#include "tools/linesweeper_capture/scripted_match.h"

#include <Windows.h>

#include <cstddef>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

using namespace labrador;
using namespace linesweeper;
using namespace mattmath;

namespace
{
	// The size both samples ask for, and the only one the sample's layout is
	// drawn for (presentation/layout.h).
	constexpr int frame_width = 1280;
	constexpr int frame_height = 720;

	// One tick's worth of presentation time. The sample pins sixty ticks a
	// second (main.cpp) and runs one tick per update, so this is the dt its
	// particle field sees on every frame the shell keeps up - and a constant
	// rather than a measurement is what makes the sparks repeatable.
	constexpr float tick_seconds = 1.0f / 60.0f;

	const std::string font_name = "courier_new_bold_16";
	const std::string block_texture_name = "white";

	class CaptureSession
	{
	public:
		// The shell is borrowed and must have loaded the sample's manifest:
		// every drawable here resolves its handles on construction.
		CaptureSession(Application* app, std::filesystem::path output,
			bool sequence);

		void run(const std::vector<capture::Command>& script);

	private:
		// What PlayState::update does after tick(): the scene steps once, at
		// the fixed dt, and its tick ends.
		void after_tick();

		void capture_frame(const capture::Command& command);
		void write_frame(const std::filesystem::path& file, bool paused);

		Application* app_ = nullptr;
		std::filesystem::path output_;
		bool sequence_ = false;
		std::size_t frames_ = 0;

		// Declared before the scene, so the World and the TickResult every
		// drawable borrows outlive them - the same order PlayState's members
		// are in.
		capture::ScriptedMatch match_;

		Scene scene_;
		ParticleField* particles_ = nullptr;

		PauseState pause_;
	};

	CaptureSession::CaptureSession(Application* app,
		std::filesystem::path output, bool sequence) :
		app_(app),
		output_(std::move(output)),
		sequence_(sequence),
		match_([this]() { this->after_tick(); }),
		scene_(nullptr, nullptr),
		pause_(app)
	{
		const Vector2F resolution =
			this->app_->resolution_manager()->resolution_vec();
		RenderResources* resources = this->app_->render_resources();

		// PlayState::init's objects, in PlayState::init's order, which is the
		// sample's whole depth system: the board, the sparks over it, the
		// banner over the sparks, then the hints.
		std::ignore = this->scene_.add(
			std::make_unique<BoardView>(&this->match_.world(), resources));

		this->particles_ = this->scene_.add(std::make_unique<ParticleField>(
			&this->match_.world(), &this->match_.last_tick(),
			resources->resolve_texture(block_texture_name)));

		std::ignore = this->scene_.add(
			std::make_unique<TopOutBanner>(&this->match_.world(), resources));

		std::ignore = this->scene_.add(std::make_unique<Label>(
			L"keyboard   arrows move   Z X rotate   space drops   C holds",
			font_name, Vector2F(344.0f, resolution.y - 60.0f), resources,
			Colour::dark_gray));

		std::ignore = this->scene_.add(std::make_unique<Label>(
			L"pad        d-pad moves   A B rotate   Y drops       X holds",
			font_name, Vector2F(344.0f, resolution.y - 36.0f), resources,
			Colour::dark_gray));

		this->scene_.end_tick();
		this->scene_.add_view(Viewport(RectangleF(Vector2F::ZERO, resolution)));

		// Built once and only ever drawn. Its update() is never called, so
		// the menu stays as it opens: the first row focused, nothing pressed.
		this->pause_.init();
	}

	void CaptureSession::run(const std::vector<capture::Command>& script)
	{
		for (const capture::Command& command : script)
		{
			if (command.kind == capture::CommandKind::capture)
			{
				this->capture_frame(command);
			}
			else
			{
				this->match_.run(command);
			}
		}

		if (this->sequence_)
		{
			std::printf("Sequence: %zu frames at 60 fps (%.3f seconds).\n",
				this->frames_, static_cast<double>(this->frames_) / 60.0);
		}
	}

	void CaptureSession::after_tick()
	{
		this->scene_.update(tick_seconds);
		this->scene_.end_tick();

		if (this->sequence_)
		{
			// The World stops counting at top-out, while its presentation keeps
			// moving. Number presentation updates, including that final burst.
			this->write_frame(this->output_ / "frames" /
				std::format("{:06}.png", this->frames_), false);
			++this->frames_;
		}
	}

	void CaptureSession::capture_frame(const capture::Command& command)
	{
		const World& world = this->match_.world();

		// PlayState refuses to open the menu over a finished match, so that
		// frame is not one the sample can show.
		if (command.paused && world.topped_out != 0)
		{
			throw std::runtime_error("script line " +
				std::to_string(command.line) + ": the sample does not open "
				"its pause menu over a finished match, so " + command.file +
				" would be a frame it never draws.");
		}

		this->write_frame(this->output_ / command.file, command.paused);

		std::printf("%-28s tick %5u  score %6u  lines %3u  level %2u  "
			"%5d particles%s%s\n",
			command.file.c_str(), world.tick, world.score, world.lines,
			static_cast<unsigned int>(world.level) + 1u,
			this->particles_->live(),
			world.topped_out != 0 ? "  topped out" : "",
			command.paused ? "  paused" : "");
	}

	void CaptureSession::write_frame(const std::filesystem::path& file,
		bool paused)
	{
		// Application::render's frame, with the read-back between submit and
		// end_frame, which is the one interval every backend can keep
		// (renderer.h).
		Renderer& renderer = *this->app_->renderer();
		renderer.begin_frame();
		this->scene_.draw(renderer);
		if (paused)
		{
			this->pause_.draw(renderer);
		}
		renderer.submit();

		std::vector<unsigned char> pixels;
		renderer.read_back_buffer(pixels);
		renderer.end_frame();

		capture::write_png(file, frame_width, frame_height, pixels);
	}

	void print_usage()
	{
		std::fprintf(stderr,
			"usage: LineSweeperCapture <output-directory> [<script>] [--sequence]\n"
			"  --sequence also writes every 60 Hz update to frames/000000.png, ...\n"
			"  The script defaults to the checked-in one:\n"
			"  %s\n", LINESWEEPER_CAPTURE_SCRIPT);
	}
}

int wmain(int argc, wchar_t* argv[])
{
	const bool sequence = argc > 1 &&
		std::wstring_view(argv[argc - 1]) == L"--sequence";
	if (sequence)
	{
		--argc;
	}

	if (argc < 2 || argc > 3)
	{
		print_usage();
		return 2;
	}

	try
	{
		const std::filesystem::path output = argv[1];
		const std::filesystem::path script_path = argc == 3
			? std::filesystem::path(argv[2])
			: std::filesystem::path(LINESWEEPER_CAPTURE_SCRIPT);

		// The whole script is read and checked before a window exists.
		const std::vector<capture::Command> script =
			capture::read_script(script_path);

		const std::filesystem::path frames = output / "frames";
		if (sequence && std::filesystem::exists(frames) &&
			!std::filesystem::is_empty(frames))
		{
			throw std::runtime_error(frames.string() +
				" is not empty; use a fresh directory for a complete sequence.");
		}

		ApplicationOptions options;
		options.window_class_name = L"LineSweeperCaptureWindowClass";
		options.window_title = L"LineSweeper capture";
		options.resolution = ScreenResolution::s_1280_720;
		options.view_capacity = 1;

		// The shell's window, device and services, and none of its loop:
		// run() is never called, so nothing steps but the script. The window
		// is never shown, because nothing is presented to anybody.
		Application app(std::move(options));
		app.initialize(GetModuleHandleW(nullptr), SW_HIDE);
		app.load_manifest("./manifest.json");

		const Vector2F size = app.renderer()->back_buffer_size();
		if (size.x != static_cast<float>(frame_width) ||
			size.y != static_cast<float>(frame_height))
		{
			throw std::runtime_error("The back buffer is " +
				std::to_string(size.x) + "x" + std::to_string(size.y) +
				", not 1280x720, so these would not be the sample's frames.");
		}

		const RenderDeviceInfo& device = app.renderer()->device_info();
		std::printf("%s on %s (%s)\n", device.backend.c_str(),
			device.device_name.c_str(),
			device.kind == RenderDeviceKind::hardware
				? "hardware"
				: "software");

		std::filesystem::create_directories(output);
		if (sequence)
		{
			std::filesystem::create_directories(frames);
		}

		CaptureSession session(&app, output, sequence);
		session.run(script);
		return 0;
	}
	catch (const std::exception& error)
	{
		std::fprintf(stderr, "LineSweeperCapture refused: %s\n", error.what());
		return 1;
	}
}
