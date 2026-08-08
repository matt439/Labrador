#pragma once

#include "engine/assets/resource_loader.h"
#include "engine/audio/audio_resources.h"
#include "engine/collision/partitioner.h"
#include "engine/core/state_context.h"
#include "engine/core/step_timer.h"
#include "engine/core/thread_pool.h"
#include "engine/render/device_resources.h"
#include "engine/render/render_resources.h"
#include "engine/render/resolution_manager.h"
#include "engine/render/screen_resolution.h"
#include "engine/render/viewport_manager.h"
#include "engine/math/matt_math.h"
#include <Audio.h>
#include <CommonStates.h>
#include <GamePad.h>
#include <SpriteBatch.h>
#include <Windows.h>
#include <memory>
#include <string>
#include <vector>

namespace artattack
{
	// What a game hands the shell before it opens a window. Everything here is a
	// decision only the game can make - its title, the resolution it read out of
	// its own save file - which is exactly why none of it is compiled into the
	// engine (T1).
	struct ApplicationOptions
	{
		// The window class name has to be unique per process, so a game that ever
		// opens two windows needs two of these.
		std::wstring window_class_name = L"ArtAttackWindowClass";
		std::wstring window_title = L"ArtAttack";

		ScreenResolution resolution = ScreenResolution::s_1280_720;
		bool fullscreen = false;

		// The fixed step the simulation advances at. Rendering is not capped by it.
		int target_fps = 60;

		// The render thread pool. max_threads also sizes the deferred contexts and
		// sprite batches, so it is the widest the renderer can ever go.
		int min_threads = 1;
		int max_threads = 16;

		// Smallest the user may drag the window; below this the swap chain is not
		// worth resizing.
		int min_window_width = 320;
		int min_window_height = 200;

		// Throws std::invalid_argument naming the field, rather than letting
		// a bad number reach the code that divides by it. Application calls
		// this before it opens a window.
		//
		// Nothing checked these. max_threads reaches Partitioner as a divisor
		// on every frame of every view, and target_fps reaches StepTimer as
		// one, so a zero in either was a hang or a crash on the first frame -
		// and the game reads both from a save file it does not control.
		void validate() const;
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
	//     app.initialize(instance, show_command);   // window, device, services
	//     app.resource_loader()->register_kind(...); // the game's own asset kinds
	//     app.load_manifest("./manifest.json");
	//     return app.run(std::make_unique<MyFirstState>(...));
	//
	// The first state is constructed last on purpose: states build drawables, and
	// a drawable resolves handles against resources that do not exist until the
	// manifest has been walked.
	class Application : public IDeviceNotify, public StateContext
	{
	public:
		explicit Application(ApplicationOptions options);
		~Application() override;

		Application(const Application&) = delete;
		Application& operator=(const Application&) = delete;

		// Opens the window, creates the device, and builds the services. Throws
		// std::runtime_error naming the step that failed (T6).
		void initialize(HINSTANCE instance, int show_command);

		// Loads everything the manifest names. Register any game-specific kinds
		// before calling this, or the walk throws naming the kind it does not know.
		void load_manifest(const std::string& manifest_path);

		// Takes the first state and runs until the window closes. Returns the
		// process exit code.
		int run(std::unique_ptr<State> first_state);

		// Closes the window, which ends run(). This is the whole of the quit path:
		// a game asking to exit does not need to know it is on Win32.
		void quit() const;

		// Resizes the window and re-points the resolution manager at the new size.
		// The game decides which resolution and when - it is reading its own
		// options menu - and the engine owns what that means to a window.
		void set_resolution(ScreenResolution resolution);

		// Switches between a borderless full-screen window and an ordinary one.
		void set_fullscreen(bool fullscreen);

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
		DeviceResources* device_resources() const;
		RenderResources* render_resources() const;
		AudioResources* audio_resources() const;
		ResourceLoader* resource_loader() const;
		ResolutionManager* resolution_manager() const;
		ViewportManager* viewport_manager() const;
		ThreadPool* thread_pool() const;
		const Partitioner* partitioner() const;
		DirectX::GamePad* gamepad() const;
		HWND window() const;

		// Recreated on device loss, so these change identity across one. The
		// vector's own address is stable; only the pointers inside it move.
		DirectX::CommonStates* common_states() const;
		std::vector<DirectX::SpriteBatch*>* sprite_batches() const;

		// Seconds the last fixed step covered. A pointer because the objects that
		// read it are built once and outlive any particular frame; the value
		// behind it is rewritten every update.
		const float* dt() const;

	private:
		// DECLARATION ORDER IS LOAD-BEARING BELOW THIS LINE.
		//
		// Members destruct in reverse declaration order. DirectXTK requires the
		// AudioEngine to outlive every WaveBank and SoundEffectInstance - their
		// destructors unregister themselves from it - and AudioResources owns the
		// SoundBanks that own those. So audio_engine_ is declared FIRST and dies
		// LAST.
		std::unique_ptr<DirectX::AudioEngine> audio_engine_ = nullptr;

		ApplicationOptions options_;
		HWND window_ = nullptr;
		std::unique_ptr<DeviceResources> device_resources_ = nullptr;
		StepTimer timer_ = StepTimer();

		std::unique_ptr<AudioResources> audio_resources_ = nullptr;
		std::unique_ptr<RenderResources> render_resources_ = nullptr;
		std::unique_ptr<ResourceLoader> resource_loader_ = nullptr;
		std::unique_ptr<ResolutionManager> resolution_manager_ = nullptr;
		std::unique_ptr<ViewportManager> viewport_manager_ = nullptr;
		std::unique_ptr<ThreadPool> thread_pool_ = nullptr;
		std::unique_ptr<Partitioner> partitioner_ = nullptr;
		std::unique_ptr<DirectX::GamePad> gamepad_ = nullptr;
		std::unique_ptr<float> dt_ = nullptr;

		std::unique_ptr<DirectX::CommonStates> common_states_ = nullptr;
		std::vector<std::unique_ptr<DirectX::SpriteBatch>> sprite_batches_;
		std::vector<DirectX::SpriteBatch*> sprite_batch_ptrs_;

		bool com_initialized_ = false;
		bool content_loaded_ = false;

		// The window forwards these; nothing else calls them.
		void tick();
		void update();
		void render();
		void clear() const;

		void on_activated() const;
		void on_deactivated() const;
		void on_suspending() const;
		void on_resuming();
		void on_window_moved() const;
		void on_display_change() const;
		void on_window_size_changed(int width, int height);

		void create_window(HINSTANCE instance, int show_command);
		void create_services();

		// Only the objects the device owns. Re-run on every restore.
		void create_device_dependent_resources();

		// IDeviceNotify
		void OnDeviceLost() override;
		void OnDeviceRestored() override;

		static LRESULT CALLBACK window_proc(HWND window, UINT message,
			WPARAM w_param, LPARAM l_param);
	};
}
