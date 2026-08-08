#pragma once

#include "engine/core/state.h"
#include <memory>

namespace artattack
{
	class StateContext
	{
	public:
		virtual ~StateContext() = default;
		StateContext() = default;
		void update();
		void draw() const;

		// Replaces the current state. Safe to call from inside the current
		// state's own update().
		//
		// It was not, and forty-two call sites depended on that going
		// unnoticed. Assigning over state_ destroys the object whose update()
		// is on the stack, so every statement after the call - including the
		// implicit ones at the end of the function - runs on freed memory.
		// Ten call sites do exactly this and are safe only because none of
		// them happens to touch a member afterwards, which is a property of
		// the call sites rather than of this function.
		//
		// So: called while this context has something of its own on the stack
		// - its state's update(), or its state's init() - the incoming state
		// is parked and swapped in once that returns. Called from anywhere
		// else, including construction and another context's update, it takes
		// effect immediately, because there is nothing to outlive. Either way
		// the new state's set_context() and init() run before its first
		// update() and before the next draw(), which is what the immediate
		// version guaranteed and what the call sites are written against.
		//
		// Two transitions in one update: the last one wins, and the ones
		// before it never became live, so their init() never runs.
		void transition_to(std::unique_ptr<State> state);
	private:
		std::unique_ptr<State> state_ = nullptr;
		std::unique_ptr<State> pending_ = nullptr;

		// True while something of this context's is on the stack - see
		// transition_to().
		bool deferring_ = false;

		void apply_pending();
	};
}
