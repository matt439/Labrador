#include <doctest/doctest.h>

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"

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
			this->steps_.push_back(this->app_->step_position());

			if (this->updates_ == capture_at)
			{
				this->capture_before_ = this->app_->frame_timings().frame;
				this->capturing_ = true;
				Renderer& renderer = *this->app_->renderer();
				renderer.begin_frame();
				this->app_->draw(renderer);
				renderer.submit();
				this->capturing_ = false;
				this->capture_after_ = this->app_->frame_timings().frame;
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

		std::uint64_t capture_before_ = 0;
		std::uint64_t capture_after_ = 0;

		// What step_position() said inside each update.
		std::vector<StepPosition> steps_;

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

	ApplicationOptions options;
	options.window_class_name = L"QuitDuringUpdateTestWindow";
	options.visible = false;
	options.view_capacity = 1;
	options.max_threads = 1;
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
	ApplicationOptions options;
	options.window_class_name = L"FrameTimingsTestWindow";
	options.visible = false;
	options.view_capacity = 1;
	options.max_threads = 1;
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
	CHECK(state->capture_after_ == state->capture_before_);

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
	ApplicationOptions options;
	options.window_class_name = L"StepPositionTestWindow";
	options.visible = false;
	options.view_capacity = 1;
	options.max_threads = 1;
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
	REQUIRE(state->steps_.size() == static_cast<size_t>(TimingState::update_count));
	for (size_t index = 0; index < state->steps_.size(); index++)
	{
		CHECK(state->steps_[index].step == index + 1);
		CHECK(state->steps_[index].fraction == 0.0f);
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
			position.step > state->steps_.back().step ||
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
	ApplicationOptions options;
	options.window_class_name = L"WhiteTextureTestWindow";
	options.visible = false;
	options.view_capacity = 1;
	options.max_threads = 1;

	Application app(std::move(options));
	app.initialize();

	CHECK_NOTHROW(std::ignore =
		app.render_resources()->resolve_texture(white_texture_name));
}
