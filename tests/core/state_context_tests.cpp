#include <doctest/doctest.h>

#include "engine/core/state_context.h"
#include "engine/render/renderer.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using artattack::Renderer;
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
		void update(float /*dt*/) override
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
		void draw(Renderer& /*renderer*/) const override
		{
			this->log_->push_back(this->name_ + ":draw");
		}

		bool covers_screen() const override
		{
			return this->covers_screen_;
		}
		void on_suspend() override
		{
			this->log_->push_back(this->name_ + ":suspend");
		}
		void on_resume() override
		{
			this->log_->push_back(this->name_ + ":resume");
		}

		void on_init(std::function<void()> action)
		{
			this->on_init_ = std::move(action);
		}
		void on_update(std::function<void()> action)
		{
			this->on_update_ = std::move(action);
		}
		void set_covers_screen(bool covers_screen)
		{
			this->covers_screen_ = covers_screen;
		}

	private:
		std::string name_;
		std::vector<std::string>* log_ = nullptr;
		std::function<void()> on_init_;
		std::function<void()> on_update_;
		bool covers_screen_ = true;
		bool touched_ = false;
	};

	// Two unrelated result types, so "popped with the wrong type" is a case the
	// tests can actually reach.
	enum class Answer
	{
		yes,
		no,
	};
	enum class Colour
	{
		red,
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
	// Constructible with no device: the seam's whole point, and this test
	// is the first thing in the tree to depend on it.
	Renderer renderer;

	context.transition_to(std::make_unique<RecordingState>("a", &log));

	// init() has already run, so the state is drawable before the first
	// update. Every nested StateContext in the game is built this way.
	CHECK(log == std::vector<std::string>{"a:init"});

	context.draw(renderer);
	CHECK(log.back() == "a:draw");
}

TEST_CASE("a state transitioning from its own update survives the call")
{
	std::vector<std::string> log;
	StateContext context;
	// Constructible with no device: the seam's whole point, and this test
	// is the first thing in the tree to depend on it.
	Renderer renderer;

	auto outgoing = std::make_unique<RecordingState>("a", &log);
	RecordingState* outgoing_raw = outgoing.get();
	context.transition_to(std::move(outgoing));

	outgoing_raw->on_update([&]()
		{
			context.transition_to(std::make_unique<RecordingState>("b", &log));
		});

	context.update(0.0f);

	// The whole point: "a" finished its own update() before it was destroyed.
	CHECK(ran_before(log, "a:update-enter", "a:update-exit"));
	CHECK(ran_before(log, "a:update-exit", "a:dtor"));

	// And the incoming state is live and initialised by the time update()
	// returns, so the draw that follows in the same frame draws "b".
	CHECK(ran_before(log, "a:dtor", "b:init"));
	log.clear();
	context.draw(renderer);
	CHECK(log == std::vector<std::string>{"b:draw"});
}

TEST_CASE("the last transition in one update wins")
{
	std::vector<std::string> log;
	StateContext context;
	// Constructible with no device: the seam's whole point, and this test
	// is the first thing in the tree to depend on it.
	Renderer renderer;

	auto outgoing = std::make_unique<RecordingState>("a", &log);
	RecordingState* outgoing_raw = outgoing.get();
	context.transition_to(std::move(outgoing));

	outgoing_raw->on_update([&]()
		{
			context.transition_to(std::make_unique<RecordingState>("b", &log));
			context.transition_to(std::make_unique<RecordingState>("c", &log));
		});

	log.clear();
	context.update(0.0f);

	// "b" never became live, so it never ran init().
	CHECK(std::find(log.begin(), log.end(), "b:init") == log.end());
	CHECK(std::find(log.begin(), log.end(), "c:init") != log.end());

	log.clear();
	context.draw(renderer);
	CHECK(log == std::vector<std::string>{"c:draw"});
}

TEST_CASE("a state transitioning from its own init survives the call")
{
	std::vector<std::string> log;
	StateContext context;
	// Constructible with no device: the seam's whole point, and this test
	// is the first thing in the tree to depend on it.
	Renderer renderer;

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
	context.draw(renderer);
	CHECK(log == std::vector<std::string>{"b:draw"});
}

TEST_CASE("an empty context updates and draws without a state")
{
	StateContext context;
	// Constructible with no device: the seam's whole point, and this test
	// is the first thing in the tree to depend on it.
	Renderer renderer;
	context.update(0.0f);
	context.draw(renderer);
	CHECK(context.depth() == 0);
}

TEST_CASE("a push suspends what is below it and only the top updates")
{
	std::vector<std::string> log;
	StateContext context;
	Renderer renderer;

	context.transition_to(std::make_unique<RecordingState>("level", &log));

	auto menu = std::make_unique<RecordingState>("menu", &log);
	// A pause menu is a box over a running match, so the match under it keeps
	// drawing.
	menu->set_covers_screen(false);
	context.push(std::move(menu));

	CHECK(context.depth() == 2);
	CHECK(ran_before(log, "level:suspend", "menu:init"));

	log.clear();
	context.update(0.0f);
	CHECK(log == std::vector<std::string>{"menu:update-enter", "menu:update-exit"});

	log.clear();
	context.draw(renderer);
	CHECK(log == std::vector<std::string>{"level:draw", "menu:draw"});
}

TEST_CASE("a state that covers the screen hides everything below it")
{
	std::vector<std::string> log;
	StateContext context;
	Renderer renderer;

	context.transition_to(std::make_unique<RecordingState>("menu", &log));
	// A match pushed over the menu that started it: the menu is still there to
	// come back to, and none of it is drawn while the match is up.
	context.push(std::make_unique<RecordingState>("level", &log));

	log.clear();
	context.draw(renderer);
	CHECK(log == std::vector<std::string>{"level:draw"});
}

TEST_CASE("pop hands a typed result to whoever pushed the state")
{
	std::vector<std::string> log;
	StateContext context;

	context.transition_to(std::make_unique<RecordingState>("level", &log));

	std::vector<Answer> answers;
	context.push<Answer>(std::make_unique<RecordingState>("menu", &log),
		[&](const Answer& answer) { answers.push_back(answer); });

	context.pop(Answer::no);

	CHECK(answers == std::vector<Answer>{Answer::no});
	CHECK(context.depth() == 1);

	// The popped state is destroyed first, then the state below is told it is
	// the top again, and only then does the callback run - so the callback sees
	// a stack that has finished changing shape.
	CHECK(ran_before(log, "menu:dtor", "level:resume"));
}

TEST_CASE("a state popping from its own update survives the call")
{
	std::vector<std::string> log;
	StateContext context;

	context.transition_to(std::make_unique<RecordingState>("level", &log));

	auto menu = std::make_unique<RecordingState>("menu", &log);
	RecordingState* menu_raw = menu.get();

	std::vector<Answer> answers;
	context.push<Answer>(std::move(menu),
		[&](const Answer& answer) { answers.push_back(answer); });

	menu_raw->on_update([&]() { context.pop(Answer::yes); });

	log.clear();
	context.update(0.0f);

	// The same hazard transition_to had, one shape along: pop destroys the
	// state whose update() is on the call stack.
	CHECK(ran_before(log, "menu:update-enter", "menu:update-exit"));
	CHECK(ran_before(log, "menu:update-exit", "menu:dtor"));
	CHECK(answers == std::vector<Answer>{Answer::yes});
	CHECK(context.depth() == 1);
}

TEST_CASE("a transition inside a pushed frame keeps the frame's result channel")
{
	std::vector<std::string> log;
	StateContext context;

	context.transition_to(std::make_unique<RecordingState>("level", &log));

	std::vector<Answer> answers;
	context.push<Answer>(std::make_unique<RecordingState>("initial", &log),
		[&](const Answer& answer) { answers.push_back(answer); });

	// The pause menu's confirmation page replacing its initial page. The
	// callback belongs to the frame, not to the page that happened to be in it,
	// which is why it is still there to answer with.
	context.transition_to(std::make_unique<RecordingState>("confirm", &log));
	CHECK(context.depth() == 2);

	context.pop(Answer::yes);

	CHECK(answers == std::vector<Answer>{Answer::yes});
	CHECK(context.depth() == 1);
}

TEST_CASE("popping with a type the push did not ask for throws")
{
	std::vector<std::string> log;
	StateContext context;

	context.transition_to(std::make_unique<RecordingState>("level", &log));
	context.push<Answer>(std::make_unique<RecordingState>("menu", &log),
		[](const Answer&) {});

	CHECK_THROWS_AS(context.pop(Colour::red), std::logic_error);
}

TEST_CASE("popping an empty stack throws where it was written")
{
	StateContext context;
	CHECK_THROWS_AS(context.pop(), std::logic_error);
}

TEST_CASE("the shape the game is built out of")
{
	// menu -> match -> pause menu -> confirmation, each frame answering in its
	// own type, and every operation issued from inside a state's own update().
	// The individual mechanisms are pinned above; this is the composition, and
	// it is the one thing about C3 that cannot be driven from a keyboard - the
	// paint-shooter reaches it only through a gamepad.
	std::vector<std::string> log;
	StateContext context;
	Renderer renderer;

	// The menu, at the bottom for the whole session.
	auto menu = std::make_unique<RecordingState>("menu", &log);
	RecordingState* menu_raw = menu.get();
	context.transition_to(std::move(menu));

	std::vector<Answer> match_results;
	RecordingState* match_raw = nullptr;

	// The menu starts a match, from its own update().
	menu_raw->on_update([&]()
		{
			auto match = std::make_unique<RecordingState>("match", &log);
			match_raw = match.get();
			context.push<Answer>(std::move(match),
				[&](const Answer& answer) { match_results.push_back(answer); });
		});
	context.update(0.0f);
	menu_raw->on_update(nullptr);

	CHECK(context.depth() == 2);
	CHECK(ran_before(log, "menu:suspend", "match:init"));

	// A match fills the frame, so the menu under it is not drawn.
	log.clear();
	context.draw(renderer);
	CHECK(log == std::vector<std::string>{"match:draw"});

	// The match opens a pause menu, which does not fill the frame. Quitting from
	// it leaves the match too, and that is a decision the *match* makes when it
	// is told what the menu answered - the menu knows nothing about it.
	RecordingState* pause_raw = nullptr;
	std::vector<Colour> pause_results;
	match_raw->on_update([&]()
		{
			auto pause = std::make_unique<RecordingState>("pause", &log);
			pause->set_covers_screen(false);
			pause_raw = pause.get();
			context.push<Colour>(std::move(pause),
				[&](const Colour& colour)
				{
					pause_results.push_back(colour);
					context.pop(Answer::no);
				});
		});
	context.update(0.0f);
	match_raw->on_update(nullptr);

	CHECK(context.depth() == 3);
	log.clear();
	context.draw(renderer);
	CHECK(log == std::vector<std::string>{"match:draw", "pause:draw"});

	// Its confirmation page replaces it in the same frame. The frame is still
	// the pause menu's, and so is the result channel.
	RecordingState* confirm_raw = nullptr;
	pause_raw->on_update([&]()
		{
			auto confirm = std::make_unique<RecordingState>("confirm", &log);
			confirm->set_covers_screen(false);
			confirm_raw = confirm.get();
			context.transition_to(std::move(confirm));
		});
	context.update(0.0f);

	CHECK(context.depth() == 3);
	REQUIRE(confirm_raw != nullptr);

	// "Quit": the confirmation answers the pause frame, and the match leaves in
	// the same drain, answering the menu in a different type entirely. One
	// update(), two frames unwound, two result types, and no state touched
	// after it was destroyed.
	log.clear();
	confirm_raw->on_update([&]() { context.pop(Colour::red); });

	context.update(0.0f);

	CHECK(pause_results == std::vector<Colour>{Colour::red});
	CHECK(match_results == std::vector<Answer>{Answer::no});
	CHECK(context.depth() == 1);

	// Both frames unwound in one update, and the menu is drawing again.
	log.clear();
	context.draw(renderer);
	CHECK(log == std::vector<std::string>{"menu:draw"});
}
