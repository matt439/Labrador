#pragma once

#include "engine/core/state.h"

#include <any>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

namespace labrador
{
	// Owns a stack of states, their suspension and result delivery
	// (docs/design/PHILOSOPHY.md, Structural types). A state's destructor releases
	// what it set up; states beneath a pushed screen remain alive.
	//
	// transition_to(), push() and pop() issued during this context's update,
	// init, activation walk or operation drain wait for the enclosing call to
	// return. Nested callbacks retain that protection; the drain never reenters
	// itself. Those operations outside these calls take effect immediately.
	//
	// A root State can own a separate StateContext to run code around every
	// game update and draw. It forwards update, draw and activation to the owned
	// context, including its parent's active() value during init(). Inner states
	// use their own context(); Application::depth() then counts only the root.
	// Frame capture belongs in the root's update after the owned update returns,
	// never in draw() (engine/render/renderer.h, Renderer::read_back_buffer).
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

		// Replaces the top state, or installs the first one on an empty stack.
		// Rejects nullptr with std::invalid_argument. Safe inside the current
		// state's update(): deferral keeps that state alive until its call returns.
		// The new state's context is set and init() runs before update() or draw().
		//
		// Consecutive queued transitions collapse to the last one; discarded
		// states never run init(). Pushes and pops between transitions preserve
		// their issue order, because they change which frame is being replaced.
		void transition_to(std::unique_ptr<State> state);

		// Puts a state above the current one and calls the covered state's
		// on_suspend(). It stops updating and keeps drawing if the new top does
		// not cover the screen. Rejects nullptr with std::invalid_argument;
		// deferral and entry follow transition_to().
		void push(std::unique_ptr<State> state);

		// Calls on_closed after this frame is popped, regardless of the result
		// supplied to pop(). The ordering and shutdown rule match on_result below.
		void push(std::unique_ptr<State> state, std::function<void()> on_closed);

		// Calls on_result with the typed result when this frame is popped.
		// The callback belongs to the frame and survives transition_to() replacing
		// its state, so a multi-page screen answers the push that opened it.
		//
		// The type is named at the push and checked at the pop:
		//
		//     this->context()->push<PauseChoice>(
		//         std::make_unique<PauseState>(this->app_),
		//         [this](const PauseChoice& choice) { ... });
		//
		//     // ...and, inside the pause screen, however many pages later:
		//     this->context()->pop(PauseChoice::resume);
		//
		// on_result runs after the popped state is destroyed and after the
		// resumed state's on_resume(). Missing or mismatched results throw
		// std::logic_error at delivery (T6). clear() never delivers results.
		template <typename Result>
		void push(std::unique_ptr<State> state,
			std::function<void(const Result&)> on_result);

		// Takes the top state off. Throws std::logic_error if there is nothing
		// on the stack to take.
		void pop();

		template <typename Result>
		void pop(const Result& result);

		// How many states are stacked: 1 at a title screen, and 3 with a pause
		// menu over a match over a menu.
		int depth() const;

		// Reports a foreground change to every live state, top first. Repeating
		// the current value does nothing. active() reflects the new value during
		// callbacks; covered states are notified even though they do not update.
		//
		// Stack operations issued by callbacks wait until the walk and any
		// enclosing callback or active drain return. A nested notification cannot
		// drain operations while an outer update() or init() is still running.
		void notify_activation(bool active);

		// Foreground state as last reported; initially true. A state constructed
		// after a notification receives no retrospective callback and reads this
		// in init() to learn whether it entered an inactive context.
		bool active() const;

		// Drops queued operations, then destroys live states top first without
		// delivering result callbacks. It leaves active() unchanged. Also called
		// by the destructor; repeated calls on an empty stack are harmless.
		//
		// Immediate, never deferred: do not call it from a state's update(),
		// init() or activation callback. The state whose code is running could
		// be destroyed, along with frames an activation walk has yet to visit.
		// Owners must clear states before destroying any services they borrow;
		// Application clears its stack before its service members are destroyed.
		// A throwing state destructor terminates here: teardown is noexcept (T6).
		void clear() noexcept;
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

		// Drained in issue order against the stack as it is when each operation
		// runs. Only adjacent transitions are coalesced.
		std::vector<PendingOp> pending_;

		// True while something of this context's is on the stack - a state's
		// update(), a state's init(), an activation callback, or the drain
		// itself.
		bool deferring_ = false;

		// The foreground, as last reported. See notify_activation.
		bool active_ = true;

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
