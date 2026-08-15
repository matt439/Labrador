#include <doctest/doctest.h>

#include "engine/core/state_context.h"
#include "engine/render/renderer.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using labrador::Renderer;
using labrador::State;
using labrador::StateContext;

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
		void on_activated() override
		{
			this->log_->push_back(this->name_ + ":activated");
		}
		void on_deactivated() override
		{
			this->log_->push_back(this->name_ + ":deactivated");
			if (this->on_deactivation_)
			{
				this->on_deactivation_();
			}
		}

		void on_init(std::function<void()> action)
		{
			this->on_init_ = std::move(action);
		}
		void on_update(std::function<void()> action)
		{
			this->on_update_ = std::move(action);
		}
		// Named for the event rather than the callback, because on_deactivated
		// is the override.
		void on_deactivation(std::function<void()> action)
		{
			this->on_deactivation_ = std::move(action);
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
		std::function<void()> on_deactivation_;
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

TEST_CASE("activation reaches every frame, from the top down")
{
	std::vector<std::string> log;
	StateContext context;

	context.transition_to(std::make_unique<RecordingState>("menu", &log));
	context.push(std::make_unique<RecordingState>("match", &log));
	auto pause = std::make_unique<RecordingState>("pause", &log);
	pause->set_covers_screen(false);
	context.push(std::move(pause));
	REQUIRE(context.depth() == 3);

	log.clear();
	context.notify_activation(false);

	// NOT THE TOP FRAME ALONE, which is the difference between this and every
	// other thing the stack does. The match under the pause menu is not being
	// updated and is the state holding the music and the looping weapon voice,
	// so it is both the frame furthest from the top and the one this exists
	// for.
	CHECK(log == std::vector<std::string>{
		"pause:deactivated", "match:deactivated", "menu:deactivated"});

	log.clear();
	context.notify_activation(true);
	CHECK(log == std::vector<std::string>{
		"pause:activated", "match:activated", "menu:activated"});
}

TEST_CASE("only a change is an edge, and only an edge is reported")
{
	std::vector<std::string> log;
	StateContext context;
	context.transition_to(std::make_unique<RecordingState>("level", &log));

	// A window given the foreground on creation is never told it has it.
	CHECK(context.active());

	log.clear();
	context.notify_activation(true);
	CHECK(log.empty());

	// Alt-tab away and then minimise: two messages, both meaning "not in front
	// of the player", and one piece of news. A client left to pair these up
	// itself cannot - SoundBank::pause_effect carries no depth, so the second
	// pause and the first are the same state and one resume answers both.
	context.notify_activation(false);
	context.notify_activation(false);
	CHECK(log == std::vector<std::string>{"level:deactivated"});
	CHECK(context.active() == false);

	log.clear();
	context.notify_activation(true);
	CHECK(log == std::vector<std::string>{"level:activated"});
	CHECK(context.active());
}

TEST_CASE("a state that arrives after the edge asks for the level instead")
{
	std::vector<std::string> log;
	StateContext context;

	// Told with nothing on the stack, which is reachable rather than
	// defensive: the window is up from initialize() and the first state does
	// not arrive until run(), so a player who clicks away during a manifest
	// load lands exactly here.
	context.notify_activation(false);
	CHECK(context.active() == false);

	bool active_at_init = true;
	auto level = std::make_unique<RecordingState>("level", &log);
	level->on_init([&]() { active_at_init = context.active(); });
	context.transition_to(std::move(level));

	// It cannot be told an edge that happened before it existed, and the stack
	// keeps updating in the background so states are still built there. The
	// level survives the state that was not there to hear it.
	CHECK(active_at_init == false);
	CHECK(log == std::vector<std::string>{"level:init"});
}

TEST_CASE("what a deactivated state asks for applies after the walk")
{
	std::vector<std::string> log;
	StateContext context;

	context.transition_to(std::make_unique<RecordingState>("level", &log));

	auto menu = std::make_unique<RecordingState>("menu", &log);
	RecordingState* menu_raw = menu.get();
	context.push(std::move(menu));

	// A page that closes itself when the player looks away.
	menu_raw->on_deactivation([&]() { context.pop(); });

	log.clear();
	context.notify_activation(false);

	// The walk is indexing frames_, so the pop cannot take effect inside it.
	// Every frame is told first - the one below included, which would have
	// been skipped if the stack had shrunk mid-loop - and the shape changes
	// afterwards.
	CHECK(ran_before(log, "menu:deactivated", "level:deactivated"));
	CHECK(ran_before(log, "level:deactivated", "menu:dtor"));
	CHECK(context.depth() == 1);
}

TEST_CASE("clear destroys the live states from the top down")
{
	std::vector<std::string> log;
	StateContext context;

	context.transition_to(std::make_unique<RecordingState>("menu", &log));
	context.push(std::make_unique<RecordingState>("match", &log));
	context.push(std::make_unique<RecordingState>("pause", &log));
	REQUIRE(context.depth() == 3);

	log.clear();
	context.clear();

	// THE ORDER IS THE WHOLE POINT, and it is the opposite of what
	// frames_.clear() does. The stack is ordered by dependency - what a screen
	// pushes above itself may borrow what that screen owns - so the bottom
	// state has to be the last one standing. A vector destroys front to back,
	// which would have destroyed it first.
	CHECK(log == std::vector<std::string>{
		"pause:dtor", "match:dtor", "menu:dtor"});
	CHECK(context.depth() == 0);
}

TEST_CASE("clear fires no result callbacks")
{
	std::vector<std::string> log;
	StateContext context;

	context.transition_to(std::make_unique<RecordingState>("level", &log));

	std::vector<Answer> answers;
	context.push<Answer>(std::make_unique<RecordingState>("menu", &log),
		[&](const Answer& answer) { answers.push_back(answer); });

	context.clear();

	// A callback is the answer to a pop, and a shutdown is not an answer.
	// Firing them here would re-enter push() from inside teardown - the game
	// this engine was written for closes its match frame by reopening the menu,
	// which pushes - and rebuild the stack being drained.
	CHECK(answers.empty());
	CHECK(context.depth() == 0);
}

TEST_CASE("clear on an empty context is a no-op, twice over")
{
	StateContext context;

	context.clear();
	CHECK(context.depth() == 0);
	context.clear();
	CHECK(context.depth() == 0);
}

TEST_CASE("clear drops a queued state that never became live")
{
	// The one reachable way to have something queued when clear() runs: a
	// drain that did not finish. An init() that throws carries the exception
	// out through update() and leaves whatever was issued behind it waiting, and
	// a client unwinding out of main from there destroys the Application, which
	// calls this.
	std::vector<std::string> log;
	StateContext context;

	auto level = std::make_unique<RecordingState>("level", &log);
	RecordingState* level_raw = level.get();
	context.transition_to(std::move(level));

	level_raw->on_update([&]()
		{
			auto bad = std::make_unique<RecordingState>("bad", &log);
			bad->on_init([]() { throw std::runtime_error("init failed"); });
			context.push(std::move(bad));
			context.push(std::make_unique<RecordingState>("never", &log));
		});

	log.clear();
	CHECK_THROWS_AS(context.update(0.0f), std::runtime_error);

	// "bad" reached the stack and ran a failing init. "never" was queued behind
	// it and was never entered.
	CHECK(std::find(log.begin(), log.end(), "never:init") == log.end());

	context.clear();

	// Everything constructed is destroyed, the one that never became live
	// included - and it goes first, because it is the newest thing here and the
	// likeliest to borrow from a state still on the stack.
	CHECK(ran_before(log, "never:dtor", "bad:dtor"));
	CHECK(ran_before(log, "bad:dtor", "level:dtor"));
	CHECK(context.depth() == 0);
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
