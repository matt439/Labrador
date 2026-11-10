#include <doctest/doctest.h>

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/core/state_context.h"
#include "engine/math/vector2i.h"
#include "engine/render/renderer.h"
#include "engine/render/resolution_manager.h"
#include "engine/render/resource_factory.h"

#include <Windows.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

// Application's own loop, run for real: a hidden window, the null renderer
// and null audio, so it needs no device and no audio endpoint and runs on a
// build machine.
//
// COMPILED ONLY UNDER THE TWO -null PRESETS, and never in a browser, where
// run() does not return (application.h). frame_timer_tests.cpp holds the
// arithmetic everywhere; this holds that the loop feeds it.

namespace
{
	using namespace labrador;

	// A hidden window with null render and audio, one view and one thread:
	// what every live case here starts from, before its own options.
	ApplicationOptions hidden_options(const wchar_t* class_name)
	{
		ApplicationOptions options;
		options.window_class_name = class_name;
		options.visible = false;
		options.view_capacity = 1;
		options.max_threads = 1;
		return options;
	}

	// Records what frame_timings() says from inside the loop, and quits after
	// a handful of updates. Halfway through it draws a frame of its own, the
	// way a capture does (Renderer::read_back_buffer), which the null
	// renderer can draw and cannot read back.
	class TimingState : public State
	{
	public:
		TimingState(Application* app, std::vector<FrameTimings>* seen,
			std::vector<std::uint64_t>* drawn,
			std::vector<StepPosition>* positions) :
			app_(app), seen_(seen), drawn_(drawn), positions_(positions)
		{
		}

		void init() override {}

		void update(float /*dt*/) override
		{
			// quit() closes the window, and the tick that called it still
			// runs every step the clock owes. On a loaded machine that is
			// several more, so after quitting nothing is recorded: the cases
			// below count exactly update_count.
			if (this->quit_)
			{
				return;
			}

			this->seen_->push_back(this->app_->frame_timings());
			this->updates_++;
			this->steps.push_back(this->app_->step_position());

			if (this->updates_ == capture_at)
			{
				this->capture_before = this->app_->frame_timings().frame;
				this->capturing_ = true;
				Renderer& renderer = *this->app_->renderer();
				renderer.begin_frame();
				this->app_->draw(renderer);
				renderer.submit();
				this->capturing_ = false;
				this->capture_after = this->app_->frame_timings().frame;
			}

			if (this->updates_ == update_count)
			{
				this->app_->quit();
				this->quit_ = true;
			}
		}

		// Which frame each of the loop's own draws is drawing: the one after
		// the last that was published.
		void draw(Renderer& /*renderer*/) const override
		{
			if (!this->capturing_ && !this->quit_)
			{
				this->drawn_->push_back(this->app_->frame_timings().frame);
				this->positions_->push_back(this->app_->step_position());
			}
		}

		static constexpr int capture_at = 4;
		static constexpr int update_count = 8;

		std::uint64_t capture_before = 0;
		std::uint64_t capture_after = 0;

		// What step_position() said inside each update.
		std::vector<StepPosition> steps;

	private:
		Application* app_ = nullptr;
		std::vector<FrameTimings>* seen_ = nullptr;
		std::vector<std::uint64_t>* drawn_ = nullptr;
		std::vector<StepPosition>* positions_ = nullptr;
		int updates_ = 0;
		bool capturing_ = false;
		bool quit_ = false;
	};

	struct QuitObservation
	{
		int updates = 0;
		int updates_after_quit = 0;
		int draws = 0;
		int draws_at_quit = 0;
		std::uint64_t frame_at_quit = 0;
		bool quit = false;
	};

	class QuitDuringUpdate : public State
	{
	public:
		QuitDuringUpdate(Application* app, QuitObservation* observation,
			bool wait_for_frame) :
			app_(app), observation_(observation), wait_for_frame_(wait_for_frame)
		{
		}

		void init() override
		{
			// The first tick owes several fixed steps, so quitting its first
			// update also exercises the remaining updates in that same tick.
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}

		void update(float /*dt*/) override
		{
			++this->observation_->updates;
			if (this->observation_->quit)
			{
				++this->observation_->updates_after_quit;
				return;
			}
			if (this->wait_for_frame_ && this->app_->frame_timings().frame == 0)
				return;

			this->observation_->frame_at_quit = this->app_->frame_timings().frame;
			this->observation_->draws_at_quit = this->observation_->draws;
			this->app_->quit();
			this->observation_->quit = true;
		}

		void draw(Renderer& /*renderer*/) const override
		{
			++this->observation_->draws;
		}

	private:
		Application* app_;
		QuitObservation* observation_;
		bool wait_for_frame_;
	};
}

TEST_CASE("quitting in update finishes owed steps without drawing or publishing another frame")
{
	bool wait_for_frame = false;
	SUBCASE("quit on the first update") {}
	SUBCASE("quit after completed frames") { wait_for_frame = true; }

	ApplicationOptions options = hidden_options(L"QuitDuringUpdateTestWindow");
	options.target_fps = 240;
	Application app(std::move(options));
	app.initialize();

	QuitObservation observation;
	CHECK(app.run(std::make_unique<QuitDuringUpdate>(
		&app, &observation, wait_for_frame)) == 0);
	CHECK(observation.quit);
	CHECK(app.window() == nullptr);
	CHECK(observation.draws == observation.draws_at_quit);
	CHECK(app.frame_timings().frame == observation.frame_at_quit);
	CHECK(app.step_position().step == static_cast<std::uint64_t>(observation.updates));
	if (wait_for_frame)
	{
		CHECK(observation.frame_at_quit > 0);
	}
	else
	{
		CHECK(observation.frame_at_quit == 0);
		CHECK(observation.updates_after_quit > 0);
	}
}

TEST_CASE("frame_timings follows the loop's own frames and no others")
{
	ApplicationOptions options = hidden_options(L"FrameTimingsTestWindow");
	// Fast enough that the test takes a fraction of a second.
	options.target_fps = 240;

	Application app(std::move(options));
	app.initialize();

	std::vector<FrameTimings> seen;
	std::vector<std::uint64_t> drawn;
	std::vector<StepPosition> positions;
	std::unique_ptr<TimingState> owned =
		std::make_unique<TimingState>(&app, &seen, &drawn, &positions);
	TimingState* state = owned.get();

	CHECK(app.frame_timings().frame == 0);
	CHECK(app.run(std::move(owned)) == 0);

	REQUIRE(seen.size() == static_cast<size_t>(TimingState::update_count));
	REQUIRE(drawn.size() >= 2);

	// The first update runs before any frame has been rendered.
	CHECK(seen.front().frame == 0);

	// Every one of the loop's draws is the frame after the last published,
	// so consecutive draws see consecutive counts: up by exactly one. One
	// assertion rather than one per frame, because an unpaced loop renders
	// thousands of them between eight fixed steps.
	size_t first_out_of_step = drawn.size();
	for (size_t index = 0; index < drawn.size(); index++)
	{
		if (drawn[index] != static_cast<std::uint64_t>(index))
		{
			first_out_of_step = index;
			break;
		}
	}
	CHECK(first_out_of_step == drawn.size());

	// The frame drawn inside the update is not one of the loop's.
	CHECK(state->capture_after == state->capture_before);

	for (const FrameTimings& timings : seen)
	{
		CHECK(timings.updates >= 0);
		CHECK(timings.interval.count() >= 0);
		CHECK(timings.update_time.count() >= 0);
		CHECK(timings.begin_frame_time.count() >= 0);
		CHECK(timings.draw_time.count() >= 0);
		CHECK(timings.submit_time.count() >= 0);
		CHECK(timings.end_frame_time.count() >= 0);
	}

	// The final update still sees the last completed frame.
	CHECK(seen.back().frame > 0);
	CHECK(app.frame_timings().frame >= seen.back().frame);
}

TEST_CASE("step_position is published before each of the loop's draws")
{
	ApplicationOptions options = hidden_options(L"StepPositionTestWindow");
	options.target_fps = 240;

	Application app(std::move(options));
	app.initialize();

	std::vector<FrameTimings> seen;
	std::vector<std::uint64_t> drawn;
	std::vector<StepPosition> positions;
	std::unique_ptr<TimingState> owned =
		std::make_unique<TimingState>(&app, &seen, &drawn, &positions);
	TimingState* state = owned.get();

	CHECK(app.step_position().step == 0);
	CHECK(app.step_position().fraction == 0.0f);
	CHECK(app.run(std::move(owned)) == 0);

	REQUIRE_FALSE(positions.empty());

	// Inside an update it is that update's own step, counted from one, and
	// no fraction.
	REQUIRE(state->steps.size() == static_cast<size_t>(TimingState::update_count));
	for (size_t index = 0; index < state->steps.size(); index++)
	{
		CHECK(state->steps[index].step == index + 1);
		CHECK(state->steps[index].fraction == 0.0f);
	}

	// A draw sees the step count of the updates that ran before it - at
	// least one, since nothing is drawn before the first - never going
	// backwards and never past the last, and a fraction inside [0, 1).
	std::uint64_t previous = 0;
	size_t out_of_contract = positions.size();
	for (size_t index = 0; index < positions.size(); index++)
	{
		const StepPosition& position = positions[index];
		if (position.step < 1 || position.step < previous ||
			position.step > state->steps.back().step ||
			!(position.fraction >= 0.0f) || !(position.fraction < 1.0f))
		{
			out_of_contract = index;
			break;
		}
		previous = position.step;
	}
	CHECK(out_of_contract == positions.size());

	// An unpaced loop draws many frames to one 240 Hz step, so the fraction
	// must have moved off zero on some of them.
	bool moved = false;
	for (const StepPosition& position : positions)
	{
		moved = moved || position.fraction > 0.0f;
	}
	CHECK(moved);
}

TEST_CASE("the shell puts the white texture in the table, with no manifest loaded")
{
	ApplicationOptions options = hidden_options(L"WhiteTextureTestWindow");

	Application app(std::move(options));
	app.initialize();

	CHECK_NOTHROW(std::ignore =
		app.render_resources()->resolve_texture(white_texture_name));
}

namespace
{
	// Writes down, from its destructor, whether the services it borrows were
	// still there: on_closed must release the states before them.
	class ReleaseWitness : public State
	{
	public:
		ReleaseWitness(Application* app, bool* destroyed, bool* services_alive) :
			app_(app), destroyed_(destroyed), services_alive_(services_alive)
		{
		}

		~ReleaseWitness() override
		{
			*this->destroyed_ = true;
			*this->services_alive_ = this->app_->renderer() != nullptr &&
				this->app_->audio_resources() != nullptr &&
				this->app_->keyboard() != nullptr;
		}

		ReleaseWitness(const ReleaseWitness&) = delete;
		ReleaseWitness& operator=(const ReleaseWitness&) = delete;

		void init() override {}
		void update(float /*dt*/) override {}
		void draw(Renderer& /*renderer*/) const override {}

	private:
		Application* app_;
		bool* destroyed_;
		bool* services_alive_;
	};
}

TEST_CASE("on_closed releases the states, then every service but the window")
{
	ApplicationOptions options = hidden_options(L"OnClosedTestWindow");
	Application app(std::move(options));
	app.initialize();

	bool destroyed = false;
	bool services_alive = false;
	app.transition_to(std::make_unique<ReleaseWitness>(&app, &destroyed,
		&services_alive));
	REQUIRE(app.depth() == 1);

	// What a browser window calls after it closes; a Win32 window never does.
	WindowNotify& notify = app;
	notify.on_closed();

	CHECK(destroyed);
	CHECK(services_alive);
	CHECK(app.depth() == 0);
	CHECK(app.renderer() == nullptr);
	CHECK(app.render_resources() == nullptr);
	CHECK(app.audio_resources() == nullptr);
	CHECK(app.resource_loader() == nullptr);
	CHECK(app.resolution_manager() == nullptr);
	CHECK(app.viewport_manager() == nullptr);
	CHECK(app.thread_pool() == nullptr);
	CHECK(app.partitioner() == nullptr);
	CHECK(app.gamepads() == nullptr);
	CHECK(app.keyboard() == nullptr);
	CHECK(app.mouse() == nullptr);
	CHECK(app.touch() == nullptr);
	CHECK(app.window() != nullptr);
}

namespace
{
	struct MinimizeObservation
	{
		int draws = 0;
		int draws_at_minimize = -1;
		int draws_at_restore = -1;
		std::uint64_t frame_at_minimize = 0;
		std::uint64_t frame_at_restore = 0;

		// The updates that ran between the two, whether each saw the stack
		// inactive, and the times of the first and last.
		int minimized_updates = 0;
		bool active_while_minimized = false;
		std::chrono::steady_clock::time_point first_minimized_update;
		std::chrono::steady_clock::time_point last_minimized_update;

		// The first frame the loop completed after the restore.
		bool saw_first_frame_back = false;
		std::chrono::nanoseconds first_interval_back{ 1 };
	};

	// Minimises its own hidden window once the loop has drawn, lets a
	// number of updates run, restores it, and quits once the loop has drawn
	// again.
	//
	// THE WINDOW STAYS HIDDEN THROUGHOUT. Each change is the WM_SIZE the
	// system would send, posted so that it arrives between two ticks as a
	// real one does, and the window's tracked state is what the loop reads.
	// Nothing is shown and nothing reaches the taskbar.
	class MinimizeDuringRun : public State
	{
	public:
		MinimizeDuringRun(Application* app, MinimizeObservation* observation) :
			app_(app), observation_(observation)
		{
		}

		void init() override {}

		void update(float /*dt*/) override
		{
			const HWND window = static_cast<HWND>(this->app_->window());
			switch (this->phase_)
			{
			case Phase::drawing:
				if (this->app_->frame_timings().frame > 0)
				{
					REQUIRE(PostMessageW(window, WM_SIZE, SIZE_MINIMIZED, 0)
						!= FALSE);
					this->phase_ = Phase::waiting;
				}
				break;

			case Phase::minimized:
			{
				const std::chrono::steady_clock::time_point now =
					std::chrono::steady_clock::now();
				if (this->observation_->minimized_updates == 0)
				{
					this->observation_->first_minimized_update = now;
				}
				this->observation_->last_minimized_update = now;
				this->observation_->minimized_updates++;
				this->observation_->active_while_minimized =
					this->observation_->active_while_minimized ||
					this->context()->active();

				if (this->observation_->minimized_updates == minimized_update_count)
				{
					const Vector2I size =
						this->app_->resolution_manager()->resolution_ivec();
					REQUIRE(PostMessageW(window, WM_SIZE, SIZE_RESTORED,
						MAKELPARAM(size.x, size.y)) != FALSE);
					this->phase_ = Phase::waiting;
				}
				break;
			}

			case Phase::restored:
				if (this->observation_->saw_first_frame_back)
				{
					this->app_->quit();
					this->phase_ = Phase::waiting;
				}
				break;

			// Between a posted message and its arrival, and after the quit:
			// the tick that posted may still owe steps, and they are not
			// counted.
			case Phase::waiting:
				break;
			}
		}

		void draw(Renderer& /*renderer*/) const override
		{
			++this->observation_->draws;

			// The loop's draw of the frame after the first one back sees
			// that first one published.
			const FrameTimings timings = this->app_->frame_timings();
			if (this->phase_ == Phase::restored &&
				!this->observation_->saw_first_frame_back &&
				timings.frame == this->observation_->frame_at_restore + 1)
			{
				this->observation_->saw_first_frame_back = true;
				this->observation_->first_interval_back = timings.interval;
			}
		}

		void on_deactivated() override
		{
			this->observation_->draws_at_minimize = this->observation_->draws;
			this->observation_->frame_at_minimize =
				this->app_->frame_timings().frame;
			this->phase_ = Phase::minimized;
		}

		void on_activated() override
		{
			this->observation_->draws_at_restore = this->observation_->draws;
			this->observation_->frame_at_restore =
				this->app_->frame_timings().frame;
			this->phase_ = Phase::restored;
		}

		// 72 steps at 240 Hz: three tenths of a second minimised.
		static constexpr int minimized_update_count = 72;

	private:
		enum class Phase { drawing, minimized, restored, waiting };

		Application* app_ = nullptr;
		MinimizeObservation* observation_ = nullptr;
		Phase phase_ = Phase::drawing;
	};
}

TEST_CASE("a minimised window keeps its updates at their rate and draws nothing")
{
	ApplicationOptions options = hidden_options(L"MinimizedLoopTestWindow");
	options.target_fps = 240;
	Application app(std::move(options));
	app.initialize();

	MinimizeObservation observation;
	CHECK(app.run(std::make_unique<MinimizeDuringRun>(&app, &observation)) == 0);

	// Hidden and drawing until it was minimised: visible = false is not the
	// gate.
	REQUIRE(observation.draws_at_minimize >= 1);
	CHECK(observation.frame_at_minimize >= 1);

	// Nothing drawn and no frame published from the minimise to the restore.
	REQUIRE(observation.draws_at_restore >= 0);
	CHECK(observation.draws_at_restore == observation.draws_at_minimize);
	CHECK(observation.frame_at_restore == observation.frame_at_minimize);

	// And every update still ran, on a stack that had been told.
	CHECK(observation.minimized_updates ==
		MinimizeDuringRun::minimized_update_count);
	CHECK_FALSE(observation.active_while_minimized);

	// AT THEIR RATE, HELD LOOSELY. 72 steps are 300 ms of the clock. A loop
	// whose sleeps ran past what StepTimer forgives would take seconds over
	// them; the bound leaves a loaded machine room and still tells the two
	// apart. That the loop sleeps at all is held by window_tests.cpp, and
	// for how long by frame_timer_tests.cpp.
	const std::chrono::steady_clock::duration minimized_for =
		observation.last_minimized_update - observation.first_minimized_update;
	CHECK(minimized_for < std::chrono::seconds(2));

	// Drawing resumed, and the first frame back is measured from the
	// restore: no interval, as after any time that was not the game's.
	CHECK(observation.saw_first_frame_back);
	CHECK(observation.first_interval_back == std::chrono::nanoseconds::zero());
	CHECK(observation.draws > observation.draws_at_restore);
}
