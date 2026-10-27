#pragma once

#include "engine/app/frame_timer.h"
#include "engine/app/window.h"
#include "engine/assets/resource_loader.h"
#include "engine/audio/audio_device.h"
#include "engine/audio/audio_resources.h"
#include "engine/collision/partitioner.h"
#include "engine/core/state_context.h"
#include "engine/core/step_position.h"
#include "engine/core/step_timer.h"
#include "engine/core/thread_pool.h"
#include "engine/input/gamepads.h"
#include "engine/input/keyboard.h"
#include "engine/input/mouse.h"
#include "engine/input/touch.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resolution_manager.h"
#include "engine/render/screen_resolution.h"
#include "engine/render/viewport_manager.h"
#include "engine/math/vector2i.h"
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace labrador
{
	// How many logical processors the machine reports, and never fewer than one.
	//
	// THE SHELL ASKS THE MACHINE TWO QUESTIONS, AND THIS IS ONE OF THEM - the
	// other is where the executable is (content_root.h). What this answer sizes is the thread pool and nothing else: the
	// renderer's view capacity is a property of the layout
	// (ApplicationOptions::view_capacity) and the partition count is a
	// property of the work (Scene::draw). One constant answering all three
	// conflates them.
	int default_thread_count();

	// What a game hands the shell before it opens a window. Everything here is a
	// decision only the game can make - its title, the resolution it read out of
	// its own save file - which is exactly why none of it is compiled into the
	// engine (T1).
	struct ApplicationOptions
	{
		// The window class name has to be unique per process, so a game that ever
		// opens two windows needs two of these. In a browser the page owns its
		// title and there is no class, so neither is read there.
		std::wstring window_class_name = L"LabradorWindowClass";
		std::wstring window_title = L"Labrador";

		ScreenResolution resolution = ScreenResolution::s_1280_720;
		bool fullscreen = false;

		// Whether the window is shown. A smoke test or a capture tool says
		// false and gets the whole shell - window, device, services - with
		// nothing on screen and no focus taken, set_fullscreen included. The
		// engine never shows a window created hidden; a window shown by other
		// means is outside this contract. A browser shows whatever the page
		// shows, so it is not read there.
		bool visible = true;

		// The fixed step the simulation advances at. Rendering is not capped by it.
		// Must convert to at least one StepTimer tick (at most 10,000,000 FPS).
		int target_fps = 60;

		// The render thread pool's floor and ceiling.
		//
		// The ceiling is a property of the MACHINE, so it defaults to what the
		// machine reports rather than to a number written here. A fan-out wider
		// than the box has hardware for is slower than not fanning out at all,
		// and the box least able to absorb that mistake is the one this engine
		// most wants to run on: a fixed sixteen would make four tasks plus a
		// blocked submitter out of a four-view frame on a two-core part.
		int min_threads = 1;
		int max_threads = default_thread_count();

		// The widest a frame may ever fan out, which is what sizes the
		// renderer's per-view recording state - a backend that records into
		// per-thread contexts has to make them before any frame starts.
		//
		// A PROPERTY OF THE LAYOUT, NOT OF THE MACHINE, and taking it from a
		// thread count is not merely untidy: every view costs a deferred context
		// and a sprite batch's dynamic vertex buffer, built eagerly at
		// create_device and rebuilt after every device restore, whether or not a
		// frame ever uses it. A sixteen-thread default would build sixteen of
		// them to draw one pane, out of the same memory the game runs in.
		//
		// Four is four-player split-screen. A game drawing one pane says 1 and
		// pays for one.
		int view_capacity = 4;

		// Smallest the user may drag the window; below this the swap chain is not
		// worth resizing.
		int min_window_width = 320;
		int min_window_height = 200;

		// Throws std::invalid_argument naming the field, rather than letting
		// a bad number reach the code that divides by it. Application calls
		// this before it opens a window.
		//
		// max_threads reaches Partitioner as a divisor on every frame of every
		// view, and target_fps reaches StepTimer as one, so a zero in either
		// would be a hang or a crash on the first frame - and a game may read
		// both from a save file it does not control.
		// view_capacity is the same shape: create_device throws below 1, and
		// throwing here instead names the field rather than the parameter.
		void validate() const;
	};

	// What the last completed frame cost on the CPU, as steady_clock saw it
	// (Application::frame_timings).
	//
	// DRAW AND SUBMIT TOGETHER ARE THE WORK, and how it splits between them is
	// the backend's. begin_frame_time and end_frame_time may contain waits for
	// the GPU or the display, so neither is the cost of a frame. None of this
	// is GPU time or CPU busy time, and the fields need not sum to interval.
	struct FrameTimings
	{
		// Frames completed since run() began; 0 until the first.
		std::uint64_t frame = 0;

		// The fixed steps the frame's tick ran: 0, 1, or several when the
		// clock was catching up.
		int updates = 0;

		// This tick's start minus the previous frame's. Zero on the first
		// frame and on the frame after a discontinuity - reset_elapsed_time,
		// or coming back from being hidden - because the time across one was
		// not the game's. One that arrives during a tick, as a reset after a
		// load in an update() does, zeroes that tick's frame as well, so two
		// frames in a row read zero.
		std::chrono::nanoseconds interval{};

		// The whole catch-up: the input polls, every stack update, and audio.
		std::chrono::nanoseconds update_time{};

		std::chrono::nanoseconds begin_frame_time{};

		// StateContext::draw, and any fan-out it joins.
		std::chrono::nanoseconds draw_time{};

		// submit, and the debug marker that closes the frame.
		std::chrono::nanoseconds submit_time{};

		std::chrono::nanoseconds end_frame_time{};
	};

	// The machinery every game needs and no game should write: a window, a device,
	// the services, the main loop, and the state stack. The game constructs it and
	// hands it a first state (PHILOSOPHY, Structural types) - there is no IGame to
	// implement and nothing here to subclass.
	//
	// It is used in three steps, because the game has something to say between
	// each pair:
	//
	//     Application app(options);
	//     app.initialize();                          // window, device, services
	//     app.resource_loader()->register_kind(...); // the game's own asset kinds
	//     app.load_manifest("./manifest.json");
	//     return app.run(std::make_unique<MyFirstState>(...));
	//
	// The first state is constructed last on purpose: states build drawables, and
	// a drawable resolves handles against resources that do not exist until the
	// manifest has been walked.
	class Application : public DeviceNotify, public StateContext,
		public WindowNotify
	{
	public:
		explicit Application(ApplicationOptions options);
		~Application() override;

		Application(const Application&) = delete;
		Application& operator=(const Application&) = delete;

		// Opens the window, creates the device, and builds the services. Throws
		// std::runtime_error naming the step that failed (T6).
		void initialize();

		// Loads everything the manifest names. Register any game-specific kinds
		// before calling this, or the walk throws naming the kind it does not know.
		//
		// A RELATIVE PATH IS RELATIVE TO THE EXECUTABLE, not to the working
		// directory, and a relative directory inside the manifest is relative
		// to the manifest (engine/app/content_root.h says why).
		// "./manifest.json" therefore means the file beside the game,
		// wherever the game was started from. An absolute path is
		// taken as it is.
		void load_manifest(const std::string& manifest_path);

		// Takes the first state and runs until the window closes. Returns the
		// process exit code.
		//
		// IN A BROWSER IT DOES NOT RETURN, AND THE APPLICATION MUST NOT BE A
		// LOCAL IN main. The frame loop is the browser's, so this hands it the
		// first frame and gives control back to the page by unwinding the
		// stack, main's included and destructors and all, before that frame
		// runs (Window::pump_until_quit). An Application declared in main is
		// destroyed there, and its window says so on the way out; one that
		// outlives main - a static - is the shape a web entry point has.
		// Nothing after this call in main ever runs, and quit() ends the loop
		// rather than returning here.
		int run(std::unique_ptr<State> first_state);

		// Closes the window, which ends run(). This is the whole of the quit path:
		// a game asking to exit does not need to know which platform it is on.
		// In a browser the page is told, and decides what comes next.
		// If called during update, the tick completes its remaining fixed steps
		// but does not draw or publish another completed frame.
		void quit() const;

		// THE LAYOUT SIZE FOLLOWS THE WINDOW. However the window comes to change
		// size - either call below, or a user dragging an edge, which no game
		// asked for at all - ResolutionManager is re-pointed at the new CLIENT
		// size before the renderer resizes its back buffer. All three arrive as
		// the window's size report - WM_SIZE on Windows - so
		// on_window_size_changed is the single place that holds the invariant,
		// and it cannot be got at from a game.
		//
		// WITHOUT IT, NEITHER HOLDS. A set_fullscreen that restyles to WS_POPUP
		// and shows maximized while the resolution manager still reports the last
		// requested preset has ViewportManager lay out every viewport and divider
		// for 1280x720 inside a 1440p back buffer, and the game renders into the
		// top-left corner of its own window with the rest cleared black. A
		// dragged window edge is the same door.

		// Resizes the window so the game gets `resolution` pixels of CLIENT area,
		// and re-points the resolution manager at what it actually got. The game
		// decides which resolution and when - it is reading its own options menu
		// - and the engine owns what that means to a window, frame included.
		void set_resolution(ScreenResolution resolution);

		// Switches between a borderless full-screen window and an ordinary one.
		// Going full screen adopts the monitor's size; coming back adopts the
		// resolution last requested, not the monitor size just vacated.
		void set_fullscreen(bool fullscreen);

		// Whether the window is full screen: what set_fullscreen last asked,
		// until the platform says otherwise. A browser does - a player leaves
		// with Escape, and a page in a frame may refuse - and this follows
		// what it decided, so an options menu reading it shows the truth.
		bool fullscreen() const;

		// Tells the clock that the time just spent was not gameplay.
		//
		// The step is fixed, so a long blocking call - loading a level, reading
		// a save - is not one enormous dt. It is a backlog, and the next tick
		// pays it off by running update() as many times as it takes, at full
		// speed, on a world that has not been drawn yet. A client that swallows
		// the first frame after a load instead pays for one frame and fixes
		// nothing beyond it.
		void reset_elapsed_time();

		// The CPU times of the last frame this loop completed (FrameTimings),
		// which a game cannot take itself: begin_frame, submit, end_frame and
		// the catch-up count all run inside the loop.
		//
		// WRITTEN BETWEEN FRAMES, after a frame's end_frame returns, so an
		// update or a draw reads the last frame completed before it. A copy,
		// safe to read from a draw worker.
		//
		// ONLY THE LOOP'S OWN FRAMES. A tick that renders nothing - before the
		// first update - publishes nothing, and neither does a frame that
		// throws. A frame a game draws itself, such as a capture
		// (Renderer::read_back_buffer), is not one of them, and the time it
		// takes inside an update is part of update_time.
		//
		// THE CLOCK'S RESOLUTION IS THE PLATFORM'S. In a browser steady_clock
		// is performance.now(), which a page without cross-origin isolation
		// may coarsen to about 100 microseconds, so a short phase there can
		// read as zero. Which phase holds the waits is the backend's
		// (SEAM.md#6).
		FrameTimings frame_timings() const;

		// How far between two fixed steps the frame being drawn falls
		// (StepPosition), for a draw that wants motion smoother than the step.
		//
		// WRITTEN TWICE A TICK. Before each update() it is that update's own
		// step with a fraction of 0, so an update - and a frame drawn inside
		// one, such as a capture - sees the step being run. After the tick's
		// last update and before its draw it is the last step run and how far
		// past it the frame falls. A reference to one member that stays put,
		// so a drawable may hold a pointer to it for the Application's life,
		// and safe to read from a draw worker.
		//
		// A STATE THAT WAS NOT UPDATED MUST NOT USE THE FRACTION. Only the top
		// of the stack is updated, and a match drawn under a pause menu has
		// stopped while the fraction goes on cycling: extrapolated, it shimmers
		// back and forth by up to a step's motion. That is what `step` is for -
		// a drawable that records it in update() and finds it unchanged in
		// draw() was updated on the latest step, and one that finds it moved
		// on was not, and draws where it is.
		//
		// On a display at the step's own rate the fraction barely moves from
		// frame to frame, and nothing drawn with it looks different.
		const StepPosition& step_position() const;

		// The services. Every one of these is null until initialize() has run -
		// which is what ApplicationOptions is for: anything the game needs to say
		// before there is a window to say it to belongs in the options, not in a
		// call against a service that does not exist yet.
		//
		// After that they are created once and never reseated, so an object may
		// hold one for its whole life - device loss recreates GPU objects in place
		// and leaves service identity alone (PHILOSOPHY, Services and lifetimes).
		//
		// Borrowed, every one: the Application owns them and outlives the states
		// it runs.
		//
		// THAT SENTENCE IS TRUE BECAUSE ~Application DRAINS THE STACK FIRST.
		// The states live in the StateContext base and every service below is a
		// member, so member destruction would otherwise run first and each of
		// these would be gone by the time the states holding them were
		// destroyed. ~Application calls StateContext::clear() as its first
		// statement; that is the whole of what makes a state safe to release a
		// service in its destructor, which is the teardown pattern
		// state_context.h documents.
		//
		// The tidier arrangement is for StateContext to be a member declared
		// last rather than a base, so that the ordering needs no first
		// statement. It is not taken because it removes push/pop/transition_to
		// /depth from every Application*, and PHILOSOPHY batches source breaks.
		// ~StateContext is virtual only because it is inherited from.
		Renderer* renderer() const;
		RenderResources* render_resources() const;
		AudioResources* audio_resources() const;
		ResourceLoader* resource_loader() const;
		ResolutionManager* resolution_manager() const;
		ViewportManager* viewport_manager() const;
		ThreadPool* thread_pool() const;
		const Partitioner* partitioner() const;
		Gamepads* gamepads() const;

		// The other three devices. All are polled beside the pads, once per
		// frame before any state updates, so every edge in the engine is
		// measured across the same frame boundary whichever device it came
		// from.
		//
		// They differ from Gamepads in where their data comes from and in
		// nothing a client can see: the pads are read from XInput on demand,
		// while these are fed from the window's messages as they arrive
		// (engine/input/keyboard.h says why that is forced rather than
		// chosen). The question a game asks is the same shape for all four.
		//
		// Touch is live while the player is here - the window in the
		// foreground and shown - whether or not the canvas has the keyboard,
		// because a finger reaches the canvas only by touching it, and a phone
		// may never give a canvas the keyboard at all.
		Keyboard* keyboard() const;
		Mouse* mouse() const;
		Touch* touch() const;

		// The native window, as Renderer::create_device takes it
		// (Window::handle): an HWND on Windows, the canvas in a browser.
		void* window() const;

	private:
		// DECLARATION ORDER IS LOAD-BEARING BELOW THIS LINE.
		//
		// Members destruct in reverse declaration order, and the audio device
		// owns every wave bank and every voice opened against it, so it is
		// declared FIRST and dies LAST - after the AudioResources whose banks
		// hold a borrowed pointer to it.
		//
		// WHAT ORDER THE AUDIO OBJECTS DIE IN IS NOT THE SHELL'S BUSINESS, which
		// is what the seam bought. DirectXTK requires its AudioEngine to outlive
		// every WaveBank and SoundEffectInstance; that rule is six lines from the
		// library that imposes it, in engine/audio/xaudio2/audio_device.cpp,
		// where the next backend can have a different one without this file
		// learning it.
		std::unique_ptr<AudioDevice> audio_device_ = nullptr;

		ApplicationOptions options_;
		std::unique_ptr<Window> window_ = nullptr;

		// AND THE TABLE IS DECLARED BEFORE THE RENDERER, so it dies after one.
		// render_resources.h states it as a term of the seam and says what it
		// costs to get wrong: the table holds whatever a backend calls a
		// texture, and on a backend that owns its own synchronisation that is a
		// resource the GPU may still be reading - an ID3D12Resource on D3D12, a
		// VkImage on Vulkan - whose only wait is in ~Renderer::Impl. Declared
		// the other way round, which is the order they are created in, every
		// texture would be released ahead of that wait on every normal exit,
		// and only those backends would show it.
		std::unique_ptr<RenderResources> render_resources_ = nullptr;
		std::unique_ptr<Renderer> renderer_ = nullptr;
		StepTimer timer_ = StepTimer();
		FrameTimer frame_timer_ = FrameTimer();
		FrameTimings frame_timings_ = FrameTimings();
		StepPosition step_position_ = StepPosition();

		std::unique_ptr<AudioResources> audio_resources_ = nullptr;
		std::unique_ptr<ResourceLoader> resource_loader_ = nullptr;
		std::unique_ptr<ResolutionManager> resolution_manager_ = nullptr;
		std::unique_ptr<ViewportManager> viewport_manager_ = nullptr;
		std::unique_ptr<ThreadPool> thread_pool_ = nullptr;
		std::unique_ptr<Partitioner> partitioner_ = nullptr;
		// Reader first, so it outlives the Gamepads that borrows it.
		std::unique_ptr<GamepadReader> gamepad_reader_ = nullptr;
		std::unique_ptr<Gamepads> gamepads_ = nullptr;

		// No reader above either of these, because neither has one to borrow.
		// They are fed from the window instead, which is why they are created
		// alongside the pads and not beside a backend.
		std::unique_ptr<Keyboard> keyboard_ = nullptr;
		std::unique_ptr<Mouse> mouse_ = nullptr;
		std::unique_ptr<Touch> touch_ = nullptr;

		// Whether initialize() opened a COM apartment the destructor has to
		// close. Only a Windows build has one to open.
		[[maybe_unused]] bool com_initialized_ = false;
		bool content_loaded_ = false;

		void update();

		// Fills in the marks after the update, and publishes them once the
		// frame has ended.
		void render(FrameMarks& marks);

		// WindowNotify. The window forwards these; nothing else calls them,
		// and they are private because a private override is still reachable
		// through the base pointer the window holds.
		//
		// They stay on Application rather than moving into Window with the rest
		// of the Win32, and that is a choice about layering rather than a
		// constraint - nothing below the seam needs telling. Publishing a
		// Renderer::display_changed() would free them, at the price of a
		// permanent seam method serving a call that changes nothing.
		void tick() override;

		// The four that mean "the player is looking at us" or "the player is
		// not", and the only four that reach the state stack. Non-const for
		// that reason and no other: see WindowNotify.
		void on_activated() override;
		void on_deactivated() override;
		void on_suspending() override;
		void on_resuming() override;

		// Only the keyboard and the mouse, which is all a canvas losing the
		// keyboard means: the pads and the stack are left alone.
		void on_keyboard_gained() override;
		void on_keyboard_lost() override;
		void on_fullscreen_changed(bool fullscreen) override;
		void on_window_moved() const override;
		void on_window_size_changed(int width, int height) override;

		// Straight into the devices, every one of them. Nothing is interpreted
		// on the way past: what a key means is the game's, and what a key IS
		// was decided by the window.
		void on_key_down(Key key) const override;
		void on_key_up(Key key) const override;
		void on_text(char32_t codepoint) const override;
		void on_mouse_move(int x, int y) const override;
		void on_mouse_button_down(MouseButton button) const override;
		void on_mouse_button_up(MouseButton button) const override;
		void on_mouse_capture_lost() const override;
		void on_mouse_wheel(float notches) const override;
		void on_mouse_wheel_horizontal(float notches) const override;
		void on_touch_down(uint32_t point, int x, int y) const override;
		void on_touch_move(uint32_t point, int x, int y) const override;
		void on_touch_up(uint32_t point, int x, int y) const override;
		void on_touch_cancel(uint32_t point) const override;

		// The keyboard and the mouse at once, because every reason to tell one
		// is a reason to tell the other: losing the foreground and being
		// suspended both mean the key-ups and button-ups for whatever is held
		// will be delivered somewhere else. Touch is told beside it, by
		// settle_presence, because losing the keyboard is not a reason to
		// lose the fingers. settle_presence, the two keyboard handlers and
		// initialize call this, and the alternative is the same pair of lines
		// written four times.
		void set_input_focus(bool focused) const;

		// WHAT THE WINDOW HAS SAID, AS THREE FACTS, because the platforms
		// do not agree on which of them arrive together. Foreground is
		// on_activated, shown is on_resuming, and has_keyboard_ is
		// on_keyboard_gained, which only a canvas separates from the first.
		//
		// The services follow from them rather than from whichever message
		// came last. The player is here when the window is in the foreground
		// and shown: the pads are read and the stack is told it is active.
		// The keyboard and the mouse need that and the keyboard as well. So a
		// tab shown again while the canvas does not have the keyboard gives
		// the pads back and not the keys, and a click beside a canvas takes
		// the keys and not the pads.
		bool foreground_ = true;
		bool shown_ = true;
		bool has_keyboard_ = true;

		// Brings the pads, the fed devices and the stack into line with the
		// three facts above. Touch follows the first two alone, as the pads
		// do.
		void settle_presence();

		void create_window();
		void create_services();

		// DeviceNotify
		void on_device_lost() override;
		void on_device_restored() override;
	};
}
