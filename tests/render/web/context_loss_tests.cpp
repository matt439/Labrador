#include "engine/render/colour.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"

#include "tests/render/test_window.h"

#include <emscripten/em_js.h>
#include <emscripten/emscripten.h>

#include <cstdio>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// What a lost WebGL2 context does to the renderer and its shell, in a real
// browser: RenderContextLossTests, a page run by run_page.mjs beside it.
//
// NOT A DOCTEST CASE, because the thing under test happens between frames. A
// browser tells a page its context is lost, and later that it is back, by
// events, and an event is delivered only once the page has returned to the
// browser - which a synchronous case never does. So this is a sequence of
// animation frames, one step per frame, and its checks are written out by
// hand: each failure is printed, the count is printed at the end, and the exit
// code is 0 when there were none and 1 otherwise.
//
// WHAT IT PINS, which is gl/backend.h's account of the gap:
//   - frames drawn while the context is lost are recorded and dropped, and
//     none of them throws;
//   - read_back_buffer throws while it is lost, rather than answer black;
//   - the shell hears nothing until the context is back, and then hears
//     on_device_lost and on_device_restored in that order, both from one
//     begin_frame;
//   - a texture the shell reloads in on_device_restored draws in the frame
//     that heard it, with the same handle it had before the loss;
//   - and nothing raises a WebGL error on the restored context, which this
//     page cannot see for itself: the error is drained by the next upload
//     before anything here could ask for it, so ctest runs the page under
//     run_page.mjs --browser-warnings fail, and a warning or an error the
//     browser logs fails the run.
//
// NOT PINNED: that the device's strings are read again after the restore. On
// one GPU the restored context describes itself exactly as the lost one did,
// so no check here could tell a fresh read from a stale one.
//
// The loss is asked for through WEBGL_lose_context, which is the browser's
// own way to rehearse one, on the canvas's existing context.

// The extension object is kept from the loss to the restore, because a lost
// context hands out no extensions: getExtension answers null until it is back.
EM_JS(void, labrador_lose_context, (const char* target), {
	var canvas = document.querySelector(UTF8ToString(target));
	var extension = canvas.getContext("webgl2").getExtension("WEBGL_lose_context");
	Module["labradorLoseContext"] = extension;
	extension.loseContext();
});

EM_JS(void, labrador_restore_context, (), {
	Module["labradorLoseContext"].restoreContext();
});

namespace
{
	using namespace labrador;
	using namespace mattmath;

	constexpr int BUFFER_SIZE = 64;

	// How many frames to draw into the lost context before asking for it
	// back: more than one, so that a frame between the loss event and the
	// restore is certainly drawn.
	constexpr int LOST_FRAMES = 3;

	// How long anything may take before the run fails rather than waits.
	constexpr int GIVE_UP_AFTER_FRAMES = 600;

	int failures = 0;

	void check(bool passed, const std::string& what)
	{
		if (!passed)
		{
			failures++;
			std::fprintf(stderr, "FAILED: %s\n", what.c_str());
		}
		else
		{
			std::printf("ok: %s\n", what.c_str());
		}
	}

	// A shell's half of a loss, as Application does it: release what the
	// device held, and load it again once it is back.
	class RecordingShell final : public DeviceNotify
	{
	public:
		RecordingShell(const Renderer& renderer, RenderResources& resources)
			: renderer_(renderer), resources_(resources) {}

		void on_device_lost() override
		{
			this->heard.push_back("lost");
			this->resources_.release_device_resources();
		}

		void on_device_restored() override
		{
			this->heard.push_back("restored");
			load_texture_asset(this->renderer_, this->resources_, "./content/",
				"quad");
		}

		std::vector<std::string> heard;

	private:
		const Renderer& renderer_;
		RenderResources& resources_;
	};

	enum class Step
	{
		draw_before_loss,
		draw_while_lost,
		wait_for_restore,
	};

	// Everything the frames share. Held in a static by main, because
	// emscripten_set_main_loop unwinds main's stack before the first frame,
	// destructors included (engine/app/application.h says why).
	struct Run
	{
		render_tests::TestWindow window{ BUFFER_SIZE, BUFFER_SIZE };
		RenderResources resources;
		Renderer renderer;
		std::unique_ptr<RecordingShell> shell;
		TextureHandle quad;
		Step step = Step::draw_before_loss;
		int frame = 0;
		int lost_frames = 0;
		std::vector<unsigned char> pixels;
	};

	// The quad's red texel over the whole buffer, so any pixel tells.
	void draw_red(Run& run)
	{
		run.renderer.begin_frame();
		run.renderer.set_view_count(1);
		DrawList list = run.renderer.view(0);
		list.draw_sprite(run.quad, RectangleI(0, 0, 1, 1),
			RectangleF(0.0f, 0.0f, static_cast<float>(BUFFER_SIZE),
				static_cast<float>(BUFFER_SIZE)),
			Colour::white, 0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);
		run.renderer.submit();
	}

	bool centre_is_red(Run& run)
	{
		run.renderer.read_back_buffer(run.pixels);
		const size_t centre =
			(static_cast<size_t>(BUFFER_SIZE / 2) * BUFFER_SIZE + BUFFER_SIZE / 2) * 4;
		return run.pixels.size() == static_cast<size_t>(BUFFER_SIZE) *
				BUFFER_SIZE * 4 &&
			run.pixels[centre] == 255 && run.pixels[centre + 1] == 0 &&
			run.pixels[centre + 2] == 0 && run.pixels[centre + 3] == 255;
	}

	void finish()
	{
		std::printf("%d failure(s)\n", failures);
		emscripten_force_exit(failures == 0 ? 0 : 1);
	}

	const char* target(const Run& run)
	{
		return static_cast<const char*>(run.window.handle());
	}

	void step(Run& run)
	{
		run.frame++;
		if (run.frame > GIVE_UP_AFTER_FRAMES)
		{
			check(false, "the run finished within " +
				std::to_string(GIVE_UP_AFTER_FRAMES) + " frames");
			finish();
			return;
		}

		switch (run.step)
		{
		case Step::draw_before_loss:
		{
			draw_red(run);
			check(centre_is_red(run), "the quad draws before the loss");
			run.renderer.end_frame();
			labrador_lose_context(target(run));
			run.step = Step::draw_while_lost;
			break;
		}

		case Step::draw_while_lost:
		{
			// The context is lost from the moment it was asked to be, and the
			// event saying so arrives in a later task, so the first of these
			// frames may come before the event and the rest after it. Either
			// way a frame is drawn into it as into any other.
			bool drew = true;
			try
			{
				draw_red(run);
				run.renderer.end_frame();
			}
			catch (const std::exception& error)
			{
				drew = false;
				std::fprintf(stderr, "  %s\n", error.what());
			}
			check(drew, "a frame drawn into a lost context throws nothing");

			bool refused = false;
			try
			{
				run.renderer.read_back_buffer(run.pixels);
			}
			catch (const std::runtime_error&)
			{
				refused = true;
			}
			check(refused, "read_back_buffer throws while the context is lost");
			check(run.shell->heard.empty(),
				"the shell hears nothing while the context is lost");

			run.lost_frames++;
			if (run.lost_frames == LOST_FRAMES)
			{
				labrador_restore_context();
				run.step = Step::wait_for_restore;
			}
			break;
		}

		case Step::wait_for_restore:
		{
			// begin_frame is where the restore is heard, so the shell has
			// either heard both halves by the end of this draw or neither.
			draw_red(run);
			if (run.shell->heard.empty())
			{
				run.renderer.end_frame();
				break;
			}

			check(run.shell->heard.size() == 2 &&
				run.shell->heard[0] == "lost" &&
				run.shell->heard[1] == "restored",
				"the shell hears lost then restored, together");
			check(centre_is_red(run),
				"the reloaded quad draws in the frame that heard the restore");
			run.renderer.end_frame();
			finish();
			return;
		}
		}
	}

	void frame(void* argument)
	{
		Run& run = *static_cast<Run*>(argument);
		try
		{
			step(run);
		}
		catch (const std::exception& error)
		{
			check(false, std::string("no step throws (") + error.what() + ")");
			finish();
		}
	}
}

int main()
{
	try
	{
		static Run run;
		run.renderer.create_device(run.window.handle(), BUFFER_SIZE,
			BUFFER_SIZE, 1);
		run.renderer.set_resources(&run.resources);
		run.shell = std::make_unique<RecordingShell>(run.renderer,
			run.resources);
		run.renderer.set_device_notify(run.shell.get());

		load_texture_asset(run.renderer, run.resources, "./content/", "quad");
		run.quad = run.resources.resolve_texture("quad");

		emscripten_set_main_loop_arg(frame, &run, 0, true);
	}
	catch (const std::exception& error)
	{
		std::fprintf(stderr, "FAILED: setting up threw: %s\n", error.what());
		return 1;
	}
	return 0;
}
