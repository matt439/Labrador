#include "engine/render/colour.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"

#include "tests/render/material_transition_scene.h"
#include "tests/render/test_window.h"
#include "materials/context_material.h"
#include "materials/context_reverse_material.h"

#include <emscripten/em_js.h>
#include <emscripten/emscripten.h>

#include <algorithm>
#include <array>
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
//   - the host page's Module.onContextLost is called once, from a frame
//     rather than from the browser's event, while the context is lost, and
//     Module.onContextRestored once, after the shell has heard both halves,
//     from the frame that heard them - and neither again after that;
//   - and nothing raises a WebGL error on the restored context, which this
//     page cannot see for itself: the error is drained by the next upload
//     before anything here could ask for it, so ctest runs the page under
//     run_page.mjs --browser-warnings fail, and a warning or an error the
//     browser logs fails the run.
//
// THE SAME SEQUENCE THROUGH THE POST STAGE when the page's address carries
// through_post (RenderContextLossWithPostTests): two coloured panes draw
// through one authored material and then another, with a flat HUD crossing
// their seam. The frame that hears the restore must match the second
// material's output without reselecting it or supplying its values again.
//
// NOT PINNED: that the device's strings are read again after the restore. On
// one GPU the restored context describes itself exactly as the lost one did,
// so no check here could tell a fresh read from a stale one.
//
// The loss is asked for through WEBGL_lose_context, which is the browser's
// own way to rehearse one, on the canvas's existing context.
//
// A LOSS NOBODY ASKED FOR, before the page asks for its own, starts the
// sequence again. A hosted runner's Chrome has lost this page's context at
// its start, its GPU process gone under three pages' SwiftShader drawing at
// once; the first read-back then threw and nothing pinned above was reached.
// The page waits for the browser to give the context back, as a game would,
// lets the renderer and the shell hear both halves, forgets what they heard,
// and begins again, saying so on a line starting "unasked context loss:" -
// which is also what excuses the browser's one warning about it in
// run_page.mjs. More than max_unasked_losses of them fail the run.

// The extension object is kept from the loss to the restore, because a lost
// context hands out no extensions: getExtension answers null until it is back.
EM_JS(void, labrador_lose_context, (const char* target), {
	var canvas = document.querySelector(UTF8ToString(target));
	var extension = canvas.getContext("webgl2").getExtension("WEBGL_lose_context");
	Module["labradorLoseContext"] = extension;
	extension.loseContext();
});

// Whether the canvas's context is lost now, asked for or not.
EM_JS(int, labrador_context_is_lost, (const char* target), {
	var canvas = document.querySelector(UTF8ToString(target));
	return canvas.getContext("webgl2").isContextLost() ? 1 : 0;
});

EM_JS(void, labrador_restore_context, (), {
	Module["labradorLoseContext"].restoreContext();
});

// A synthetic lower advertised bound proves the renderer reads it again;
// a physical device normally reports the same value after restoration. This
// changes only the query, never GL's upload rules or the context's real limit.
EM_JS(void, labrador_install_texture_limit_fixture, (), {
	Module["labradorTextureLimit"] = 1024;
	var original = WebGL2RenderingContext.prototype.getParameter;
	WebGL2RenderingContext.prototype.getParameter = function (name) {
		if (name === this.MAX_TEXTURE_SIZE && !this.isContextLost())
			return Module["labradorTextureLimit"];
		return original.call(this, name);
	};
});

EM_JS(void, labrador_change_texture_limit_fixture, (), {
	Module["labradorTextureLimit"] = 512;
});

// The host page's two hooks, as a page sets them, recording each call and
// whether it came while a frame was running: 1 for lost and 2 for restored,
// plus 10 when no frame was.
EM_JS(void, labrador_install_page_hooks, (), {
	Module["labradorPageHeard"] = [];
	Module["labradorInFrame"] = false;
	var record = function (kind) {
		Module["labradorPageHeard"].push(kind + (Module["labradorInFrame"] ? 0 : 10));
	};
	Module.onContextLost = function () { record(1); };
	Module.onContextRestored = function () { record(2); };
});

EM_JS(void, labrador_set_in_frame, (int in_frame), {
	Module["labradorInFrame"] = in_frame !== 0;
});

EM_JS(void, labrador_page_heard_forget, (), {
	Module["labradorPageHeard"] = [];
});

EM_JS(int, labrador_page_heard_count, (), {
	return Module["labradorPageHeard"].length;
});

EM_JS(int, labrador_page_heard, (int index), {
	return Module["labradorPageHeard"][index];
});

namespace
{
	using namespace labrador;

	constexpr int buffer_side = 64;

	// What labrador_page_heard answers.
	constexpr int page_heard_lost = 1;
	constexpr int page_heard_restored = 2;

	// How many frames to draw into the lost context before asking for it
	// back: more than one, so that a frame between the loss event and the
	// restore is certainly drawn.
	constexpr int frames_to_lose = 3;

	// How many losses the browser may make unasked, each waited out and the
	// sequence begun again, before the run fails.
	constexpr int max_unasked_losses = 2;

	// How long anything may take before the run fails rather than waits.
	constexpr int give_up_after_frames = 600;

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
			check(!this->renderer_.texture_dimension_limit().has_value(),
				"the texture bound is unavailable in on_device_lost");
			this->heard.push_back("lost");
			this->resources_.release_device_resources();
		}

		void on_device_restored() override
		{
			check(this->renderer_.texture_dimension_limit() ==
				static_cast<std::uint32_t>(emscripten_run_script_int(
					"Module['labradorTextureLimit']")),
				"on_device_restored observes the freshly queried texture bound");
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
		recover_unasked_loss,
		draw_before_loss,
		draw_switched_before_loss,
		draw_while_lost,
		wait_for_restore,
		draw_after_restore,
	};

	// Everything the frames share. Held in a static by main, because
	// emscripten_set_main_loop unwinds main's stack before the first frame,
	// destructors included (engine/app/application.h says why).
	struct Run
	{
		render_tests::TestWindow window{ buffer_side, buffer_side };
		RenderResources resources;
		Renderer renderer;
		std::unique_ptr<RecordingShell> shell;
		TextureHandle quad;
		MaterialHandle material;
		MaterialHandle reverse_material;
		Step step = Step::draw_before_loss;
		int frame = 0;
		int lost_frames = 0;
		int unasked_losses = 0;
	};

	// The direct variant draws a single red quad.
	void draw_frame(Run& run)
	{
		run.renderer.begin_frame();
		if (run.material.valid())
		{
			render_tests::draw_material_transition_scene(run.renderer, run.quad,
				RectangleI(1, 1, 1, 1), buffer_side, buffer_side);
			// The frame whose begin_frame delivered restoration leaves a new
			// mark across the HUD seam. A replay of the last frame cannot pass.
			if (run.step == Step::wait_for_restore && !run.shell->heard.empty())
			{
				DrawList hud = run.renderer.view(2);
				hud.draw_sprite(run.quad, RectangleI(1, 1, 1, 1),
					RectangleF(31.0f, 6.0f, 2.0f, 1.0f), Colour::cyan,
					0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
			}
			run.renderer.submit();
			return;
		}
		run.renderer.set_view_count(1);
		DrawList list = run.renderer.view(0);
		list.draw_sprite(run.quad, RectangleI(0, 0, 1, 1),
			RectangleF(0.0f, 0.0f, static_cast<float>(buffer_side),
				static_cast<float>(buffer_side)),
			Colour::white, 0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
		run.renderer.submit();
	}

	using Pixel = std::array<unsigned char, 4>;
	constexpr Pixel red{ 255, 0, 0, 255 };
	constexpr Pixel green{ 0, 255, 0, 255 };
	constexpr Pixel blue{ 0, 0, 255, 255 };
	constexpr Pixel yellow{ 255, 255, 0, 255 };
	constexpr Pixel cyan{ 0, 255, 255, 255 };

	void check_frame(Run& run, const char* stage, const Pixel& left, const Pixel& right,
		bool expect_restore_mark = false)
	{
		std::vector<unsigned char> pixels;
		run.renderer.read_back_buffer(pixels);
		const size_t expected_size = static_cast<size_t>(buffer_side) * buffer_side * 4;
		if (pixels.size() != expected_size)
		{
			check(false, std::string(stage) + ": expected " + std::to_string(expected_size) +
				" read-back bytes, observed " + std::to_string(pixels.size()));
			return;
		}
		for (int y = 0; y < buffer_side; ++y)
		{
			for (int x = 0; x < buffer_side; ++x)
			{
				const bool hud = run.material.valid() && x >= buffer_side / 2 - 3 &&
					x < buffer_side / 2 + 3 && y >= 5 && y < 8;
				const Pixel& world = x < buffer_side / 2 ? left : right;
				const Pixel& background = hud ? yellow : world;
				const bool mark = expect_restore_mark && y == 6 && (x == 31 || x == 32);
				const Pixel& expected = mark ? cyan : background;
				const unsigned char* observed = pixels.data() + (y * buffer_side + x) * 4;
				if (!std::equal(expected.begin(), expected.end(), observed))
				{
					std::fprintf(stderr, "pixel (%d,%d): expected RGBA (%d,%d,%d,%d), "
						"observed (%d,%d,%d,%d)\n", x, y,
						expected[0], expected[1], expected[2], expected[3],
						observed[0], observed[1], observed[2], observed[3]);
					check(false, stage);
					return;
				}
			}
		}
		check(true, stage);
	}

	void finish()
	{
		std::printf("%d failure(s)\n", failures);
		emscripten_force_exit(failures == 0 ? 0 : 1);
	}

	// Whether the page has heard exactly `expected`, in order, each from a
	// frame.
	bool page_heard(const std::vector<int>& expected)
	{
		if (labrador_page_heard_count() != static_cast<int>(expected.size()))
		{
			return false;
		}
		for (size_t index = 0; index < expected.size(); index++)
		{
			if (labrador_page_heard(static_cast<int>(index)) != expected[index])
			{
				return false;
			}
		}
		return true;
	}

	const char* target(const Run& run)
	{
		return static_cast<const char*>(run.window.handle());
	}

	void step(Run& run)
	{
		run.frame++;
		if (run.frame > give_up_after_frames)
		{
			check(false, "the run finished within " +
				std::to_string(give_up_after_frames) + " frames");
			finish();
			return;
		}

		switch (run.step)
		{
		case Step::recover_unasked_loss:
		{
			// Drawn into as into any lost context, until the browser gives it
			// back and begin_frame tells the shell both halves.
			try
			{
				draw_frame(run);
				run.renderer.end_frame();
			}
			catch (const std::exception& error)
			{
				check(false, std::string("a frame drawn while the browser's own "
					"loss is waited out throws nothing (") + error.what() + ")");
				finish();
				return;
			}
			if (run.shell->heard.size() < 2 || labrador_context_is_lost(target(run)))
			{
				break;
			}
			std::printf("the context is back after the unasked loss; starting again\n");
			run.shell->heard.clear();
			labrador_page_heard_forget();
			if (run.material.valid())
			{
				run.renderer.set_post_material(run.material);
			}
			run.step = Step::draw_before_loss;
			break;
		}

		case Step::draw_before_loss:
		case Step::draw_switched_before_loss:
		{
			if (labrador_context_is_lost(target(run)))
			{
				run.unasked_losses++;
				std::printf("unasked context loss: the browser lost the context "
					"before the test asked for its loss (%d of at most %d); "
					"waiting for it to come back, then starting again\n",
					run.unasked_losses, max_unasked_losses);
				if (run.unasked_losses > max_unasked_losses)
				{
					check(false, "the browser lost the context unasked at most " +
						std::to_string(max_unasked_losses) + " times");
					finish();
					return;
				}
				run.step = Step::recover_unasked_loss;
				break;
			}
			draw_frame(run);
			check(run.renderer.texture_dimension_limit() == 1024u,
				"the created device reports the initial synthetic texture bound");
			if (!run.material.valid())
			{
				check_frame(run, "direct quad before loss", red, red);
			}
			else if (run.step == Step::draw_before_loss)
			{
				check_frame(run, "forward material before switch", green, red);
			}
			else
			{
				check_frame(run, "reverse material before loss", blue, green);
			}
			run.renderer.end_frame();
			if (run.material.valid() && run.step == Step::draw_before_loss)
			{
				run.renderer.set_post_material(run.reverse_material);
				PostValues reverse_values{};
				reverse_values.values[4] = 1.0f;
				run.renderer.set_post_values(run.reverse_material, reverse_values);
				run.step = Step::draw_switched_before_loss;
				break;
			}
			labrador_lose_context(target(run));
			run.step = Step::draw_while_lost;
			break;
		}

		case Step::draw_while_lost:
		{
			check(!run.renderer.texture_dimension_limit().has_value(),
				"the texture bound is unavailable while the context is lost");
			TextureData empty;
			bool refused_upload = false;
			try
			{
				add_texture_asset(run.renderer, run.resources, "lost-upload", empty);
			}
			catch (const std::runtime_error& error)
			{
				refused_upload = std::string(error.what()).find("lost-upload") !=
					std::string::npos;
			}
			check(refused_upload, "an upload while lost is refused by name");
			// The context is lost from the moment it was asked to be, and the
			// event saying so arrives in a later task, so the first of these
			// frames may come before the event and the rest after it. Either
			// way a frame is drawn into it as into any other.
			bool drew = true;
			try
			{
				draw_frame(run);
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
				std::vector<unsigned char> pixels;
				run.renderer.read_back_buffer(pixels);
			}
			catch (const std::runtime_error&)
			{
				refused = true;
			}
			check(refused, "read_back_buffer throws while the context is lost");
			check(run.shell->heard.empty(),
				"the shell hears nothing while the context is lost");
			// The event may not have arrived before this frame's
			// begin_frame, so the page has heard of the loss or nothing.
			check(page_heard({}) || page_heard({ page_heard_lost }),
				"the page hears at most the loss, once and from a frame, "
				"while the context is lost");

			run.lost_frames++;
			// Asked back only once the page has heard it went, so that the
			// order checked below is the renderer's and not the browser's.
			if (run.lost_frames >= frames_to_lose &&
				page_heard({ page_heard_lost }))
			{
				labrador_change_texture_limit_fixture();
				labrador_restore_context();
				run.step = Step::wait_for_restore;
			}
			break;
		}

		case Step::wait_for_restore:
		{
			check(!run.renderer.texture_dimension_limit().has_value(),
				"the texture bound stays unavailable until a frame rebuilds resources");
			// begin_frame is where the restore is heard, so the shell has
			// either heard both halves by the end of this draw or neither.
			draw_frame(run);
			if (run.shell->heard.empty())
			{
				run.renderer.end_frame();
				break;
			}

			check(run.shell->heard.size() == 2 &&
				run.shell->heard[0] == "lost" &&
				run.shell->heard[1] == "restored",
				"the shell hears lost then restored, together");
			check(run.renderer.texture_dimension_limit() == 512u,
				"the restored device reports the changed synthetic texture bound");
			if (run.material.valid())
			{
				check_frame(run, "reverse material and marked flat HUD on the first restored frame",
					blue, green, true);
			}
			else
			{
				check_frame(run, "reloaded direct quad on the first restored frame", red, red);
			}
			check(page_heard({ page_heard_lost, page_heard_restored }),
				"the page hears lost then restored, each once and from a "
				"frame, by the end of the frame that restored the shell");
			run.renderer.end_frame();
			run.step = Step::draw_after_restore;
			break;
		}

		case Step::draw_after_restore:
		{
			if (run.material.valid())
			{
				run.renderer.set_post_values(run.reverse_material, PostValues{});
			}
			draw_frame(run);
			check_frame(run, "frame after restore and value update", red, run.material.valid() ? blue : red);
			run.renderer.end_frame();
			check(run.shell->heard.size() == 2 &&
				page_heard({ page_heard_lost, page_heard_restored }),
				"neither the shell nor the page hears anything more");
			finish();
			return;
		}
		}
	}

	void frame(void* argument)
	{
		Run& run = *static_cast<Run*>(argument);
		labrador_set_in_frame(1);
		try
		{
			step(run);
		}
		catch (const std::exception& error)
		{
			check(false, std::string("no step throws (") + error.what() + ")");
			finish();
		}
		labrador_set_in_frame(0);
	}
}

int main()
{
	try
	{
		static Run run;
		labrador_install_texture_limit_fixture();
		run.renderer.create_device(run.window.handle(), buffer_side,
			buffer_side, 3);
		run.renderer.set_resources(&run.resources);
		if (emscripten_run_script_int(
			"new URLSearchParams(location.search).has('through_post') ? 1 : 0") != 0)
		{
			run.material = run.renderer.add_material(labrador::context_material);
			run.reverse_material = run.renderer.add_material(labrador::context_reverse_material);
			run.renderer.set_post_material(run.material);
			PostValues forward_values{};
			forward_values.values[0] = 1.0f;
			run.renderer.set_post_values(run.material, forward_values);
			std::printf("ok: every frame draws through an authored material\n");
		}
		run.shell = std::make_unique<RecordingShell>(run.renderer,
			run.resources);
		run.renderer.set_device_notify(run.shell.get());
		labrador_install_page_hooks();

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
