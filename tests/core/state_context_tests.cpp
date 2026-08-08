#include <doctest/doctest.h>

#include "engine/core/state_context.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using artattack::State;
using artattack::StateContext;

namespace
{
	// Records what ran, in order, into a log the test owns - so a state that
	// is destroyed mid-call cannot take the evidence with it.
	class RecordingState : public State
	{
	public:
		RecordingState(std::string name, std::vector<std::string>* log) :
			name_(std::move(name)), log_(log)
		{
		}
		~RecordingState() override
		{
			this->log_->push_back(this->name_ + ":dtor");
		}

		void init() override
		{
			this->log_->push_back(this->name_ + ":init");
			if (this->on_init_)
			{
				this->on_init_();
			}
		}
		void update() override
		{
			this->log_->push_back(this->name_ + ":update-enter");
			if (this->on_update_)
			{
				this->on_update_();
			}
			// The statement the ten live call sites do not have. If the
			// transition above destroyed this object, this touches freed
			// memory and the line below never appears in the log.
			this->touched_ = true;
			this->log_->push_back(this->name_ + ":update-exit");
		}
		void draw() override
		{
			this->log_->push_back(this->name_ + ":draw");
		}

		void on_init(std::function<void()> action)
		{
			this->on_init_ = std::move(action);
		}
		void on_update(std::function<void()> action)
		{
			this->on_update_ = std::move(action);
		}

	private:
		std::string name_;
		std::vector<std::string>* log_ = nullptr;
		std::function<void()> on_init_;
		std::function<void()> on_update_;
		bool touched_ = false;
	};

	bool ran_before(const std::vector<std::string>& log,
		const std::string& first, const std::string& second)
	{
		const auto a = std::find(log.begin(), log.end(), first);
		const auto b = std::find(log.begin(), log.end(), second);
		return a != log.end() && b != log.end() && a < b;
	}
}

TEST_CASE("transition_to outside update takes effect immediately")
{
	std::vector<std::string> log;
	StateContext context;

	context.transition_to(std::make_unique<RecordingState>("a", &log));

	// init() has already run, so the state is drawable before the first
	// update. Every nested StateContext in the game is built this way.
	CHECK(log == std::vector<std::string>{"a:init"});

	context.draw();
	CHECK(log.back() == "a:draw");
}

TEST_CASE("a state transitioning from its own update survives the call")
{
	std::vector<std::string> log;
	StateContext context;

	auto outgoing = std::make_unique<RecordingState>("a", &log);
	RecordingState* outgoing_raw = outgoing.get();
	context.transition_to(std::move(outgoing));

	outgoing_raw->on_update([&]()
		{
			context.transition_to(std::make_unique<RecordingState>("b", &log));
		});

	context.update();

	// The whole point: "a" finished its own update() before it was destroyed.
	CHECK(ran_before(log, "a:update-enter", "a:update-exit"));
	CHECK(ran_before(log, "a:update-exit", "a:dtor"));

	// And the incoming state is live and initialised by the time update()
	// returns, so the draw that follows in the same frame draws "b".
	CHECK(ran_before(log, "a:dtor", "b:init"));
	log.clear();
	context.draw();
	CHECK(log == std::vector<std::string>{"b:draw"});
}

TEST_CASE("the last transition in one update wins")
{
	std::vector<std::string> log;
	StateContext context;

	auto outgoing = std::make_unique<RecordingState>("a", &log);
	RecordingState* outgoing_raw = outgoing.get();
	context.transition_to(std::move(outgoing));

	outgoing_raw->on_update([&]()
		{
			context.transition_to(std::make_unique<RecordingState>("b", &log));
			context.transition_to(std::make_unique<RecordingState>("c", &log));
		});

	log.clear();
	context.update();

	// "b" never became live, so it never ran init().
	CHECK(std::find(log.begin(), log.end(), "b:init") == log.end());
	CHECK(std::find(log.begin(), log.end(), "c:init") != log.end());

	log.clear();
	context.draw();
	CHECK(log == std::vector<std::string>{"c:draw"});
}

TEST_CASE("a state transitioning from its own init survives the call")
{
	std::vector<std::string> log;
	StateContext context;

	auto first = std::make_unique<RecordingState>("a", &log);
	RecordingState* first_raw = first.get();
	first_raw->on_init([&]()
		{
			context.transition_to(std::make_unique<RecordingState>("b", &log));
		});

	context.transition_to(std::move(first));

	// "a" ran its init() to completion, then was replaced.
	CHECK(ran_before(log, "a:init", "a:dtor"));
	CHECK(ran_before(log, "a:dtor", "b:init"));

	log.clear();
	context.draw();
	CHECK(log == std::vector<std::string>{"b:draw"});
}

TEST_CASE("an empty context updates and draws without a state")
{
	StateContext context;
	context.update();
	context.draw();
}
