#pragma once

#include "engine/core/state.h"

#include <any>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

namespace artattack
{
	// The state stack: what a game's flow is made of, and the only flow
	// machinery the engine has (PHILOSOPHY, Structural types).
	//
	// IT IS A STACK BECAUSE THE ONE CLIENT ALREADY GREW ONE. This was a single
	// slot, so the paint-shooter built a stack out of four nested contexts, a
	// GameLevelState enum, three unique_ptr<enum> out-parameters and a
	// unique_ptr<bool> - a heap-allocated enum per menu, polled in a switch on
	// every frame, existing only so a child could tell its parent what the
	// player chose. All of that is what push, pop and a result are.
	//
	// It bought two bugs a player could see, and both are structural rather
	// than careless:
	//
	//  - The split-screen layout was set on the way into a match and never put
	//    back, because five separate exits each had to remember. After a
	//    four-player match the main menu was drawn four times, into
	//    quarter-screen viewports. A thing that is entered and left wants a
	//    lifetime, and now the level has one: it is *above* the menu on the
	//    stack rather than in place of it, so the menu is still there to come
	//    back to and the layout is released by ~GameLevel.
	//
	//  - Pausing paused the simulation and nothing else, because "paused" meant
	//    "the branch that calls Level::update is not taken" - and Level::update
	//    is the only thing that ever stops a looping weapon voice. The music
	//    played at full volume under the pause menu and a player who paused
	//    mid-burst left a sustained tone running. Not calling update() can never
	//    fix that; on_suspend() can, and it is on State because the state is
	//    what knows it has been covered.
	//
	// THREE OPERATIONS, AND THEY ALL DEFER. transition_to replaces the top,
	// push puts a state above it, pop takes the top off and hands a result
	// down. Every one of them is issued from inside a state's own update() or
	// init(), which is to say from inside a call frame belonging to a state one
	// of them may be about to destroy - so none of them takes effect where it
	// is written. They queue, and the queue drains once the stack's own call
	// returns. See transition_to.
	class StateContext
	{
	public:
		virtual ~StateContext();
		StateContext();

		StateContext(const StateContext&) = delete;
		StateContext& operator=(const StateContext&) = delete;

		// Updates the top state only. Everything below it is suspended.
		void update(float dt);

		// Draws from the topmost state that covers the screen, upward
		// (State::covers_screen). The level draws, then the pause menu over it,
		// in one pass with no state having to know what is under it.
		void draw(Renderer& renderer) const;

		// Replaces the top state, or installs the first one if the stack is
		// empty. Safe to call from inside the current state's own update().
		//
		// It was not, and forty-two call sites depended on that going
		// unnoticed. Assigning over the state destroys the object whose
		// update() is on the stack, so every statement after the call -
		// including the implicit ones at the end of the function - runs on
		// freed memory. Ten call sites do exactly this and are safe only
		// because none of them happens to touch a member afterwards, which is a
		// property of the call sites rather than of this function.
		//
		// So: called while this context has something of its own on the stack -
		// a state's update(), or a state's init() - the operation is queued and
		// applied once that returns. Called from anywhere else, including
		// construction, it takes effect immediately, because there is nothing
		// to outlive. Either way the new state's set_context() and init() run
		// before its first update() and before the next draw(), which is what
		// the immediate version guaranteed and what the call sites are written
		// against.
		//
		// Two transitions in one update: the last one wins, and the ones before
		// it never became live, so their init() never runs. A transition is a
		// statement about who occupies a frame, and saying it twice does not
		// make two occupants.
		void transition_to(std::unique_ptr<State> state);

		// Puts a state above the current one, which is suspended: it stops
		// being updated and keeps being drawn if the new top does not cover the
		// screen.
		void push(std::unique_ptr<State> state);

		// The same, plus "tell me when it closes". For a screen whose only exit
		// is the way out - the results screen has one - where an enum with a
		// single value would be a result type carrying no information.
		void push(std::unique_ptr<State> state, std::function<void()> on_closed);

		// The same, plus the answer to "and tell me what it decided".
		//
		// THE RESULT CHANNEL IS TYPED AND IT BELONGS TO THE FRAME, not to the
		// state. Pages replace each other inside one pushed screen - the pause
		// menu's confirmation page transitions over its initial page - so a
		// callback living on the state object would be lost by the first
		// transition. It lives on the stack frame, which is what actually
		// spans the thing that was pushed.
		//
		// The type is named at the push and checked at the pop:
		//
		//     this->context()->push<PauseMenuAction>(
		//         std::make_unique<PauseMenuInitial>(&this->menu_, player),
		//         [this](const PauseMenuAction& action) { ... });
		//
		//     // ...and, three pages later:
		//     this->context()->pop(PauseMenuAction::quit);
		//
		// on_result runs after the popped state is destroyed and after the
		// resumed state's on_resume(), so it sees a stack that has finished
		// changing shape. Popping with a type the push did not ask for throws
		// (T6) rather than reinterpreting the bytes.
		template <typename Result>
		void push(std::unique_ptr<State> state,
			std::function<void(const Result&)> on_result);

		// Takes the top state off. Throws std::logic_error if there is nothing
		// on the stack to take.
		void pop();

		template <typename Result>
		void pop(const Result& result);

		// How many states are stacked. The shell's own is 1 at a title screen
		// and 3 with a pause menu over a match over a menu.
		int depth() const;
	private:
		struct Frame
		{
			std::unique_ptr<State> state;

			// What whoever pushed this frame asked to be told when it pops.
			// Empty for the bottom frame and for a push that wanted no answer.
			std::function<void(const std::any&)> on_result;
		};

		// An operation waiting for the stack's own call to return.
		struct PendingOp
		{
			enum class Kind
			{
				transition,
				push,
				pop,
			};

			Kind kind = Kind::transition;
			std::unique_ptr<State> state;					// transition, push
			std::function<void(const std::any&)> on_result;	// push
			std::any result;								// pop
		};

		std::vector<Frame> frames_;

		// In issue order, and drained in issue order against the stack as it is
		// when each one runs. Never more than a couple long: it holds what one
		// state said during one update.
		std::vector<PendingOp> pending_;

		// True while something of this context's is on the stack - a state's
		// update(), a state's init(), or the drain itself.
		bool deferring_ = false;

		void queue(PendingOp op);
		void apply_pending();
		void apply_transition(std::unique_ptr<State> state);
		void apply_push(std::unique_ptr<State> state,
			std::function<void(const std::any&)> on_result);
		void apply_pop(const std::any& result);

		// Installs a live state: the context, then init(). Anything init()
		// asks for is queued, because the drain is deferring.
		void enter(State* state);

		void push_frame(std::unique_ptr<State> state,
			std::function<void(const std::any&)> on_result);
		void pop_frame(std::any result);
	};

	template <typename Result>
	void StateContext::push(std::unique_ptr<State> state,
		std::function<void(const Result&)> on_result)
	{
		this->push_frame(std::move(state),
			[callback = std::move(on_result)](const std::any& result)
			{
				const Result* value = std::any_cast<Result>(&result);
				if (value == nullptr)
				{
					throw std::logic_error("StateContext::pop - this state was "
						"pushed with a result callback of a different type, or "
						"popped with no result at all.");
				}
				callback(*value);
			});
	}

	template <typename Result>
	void StateContext::pop(const Result& result)
	{
		this->pop_frame(std::any(result));
	}
}
