#include "engine/core/state_context.h"

#include <memory>
#include <utility>

namespace artattack
{
	StateContext::StateContext() = default;
	StateContext::~StateContext() = default;

	void StateContext::update(float dt)
	{
		if (this->frames_.empty())
		{
			return;
		}

		// Only the top. Everything under it was told on_suspend() when it was
		// covered and will be told on_resume() when it is uncovered again.
		this->deferring_ = true;
		this->frames_.back().state->update(dt);
		this->deferring_ = false;

		// After update() returns, so a state that transitioned or popped is
		// destroyed with nothing of its own on the call stack.
		this->apply_pending();
	}

	void StateContext::draw(Renderer& renderer) const
	{
		// Walk down for the topmost state that fills the frame on its own, then
		// draw from there up. With one state that is the state; with a pause
		// menu over a match it is the match, and neither of the two has to know
		// the other is there.
		size_t first = 0;
		for (size_t i = this->frames_.size(); i > 0; i--)
		{
			if (this->frames_[i - 1].state->covers_screen())
			{
				first = i - 1;
				break;
			}
		}

		for (size_t i = first; i < this->frames_.size(); i++)
		{
			this->frames_[i].state->draw(renderer);
		}
	}

	void StateContext::transition_to(std::unique_ptr<State> state)
	{
		if (state == nullptr)
		{
			throw std::invalid_argument(
				"StateContext::transition_to(nullptr) - a context with no state "
				"is what an empty stack already is.");
		}

		PendingOp op;
		op.kind = PendingOp::Kind::transition;
		op.state = std::move(state);
		this->queue(std::move(op));
	}

	void StateContext::push(std::unique_ptr<State> state)
	{
		this->push_frame(std::move(state), nullptr);
	}

	void StateContext::push(std::unique_ptr<State> state,
		std::function<void()> on_closed)
	{
		// Whatever the pop carried is dropped, which is the contract: this
		// overload asked to be told, not to be told something.
		this->push_frame(std::move(state),
			[callback = std::move(on_closed)](const std::any&) { callback(); });
	}

	void StateContext::pop()
	{
		this->pop_frame(std::any());
	}

	void StateContext::clear() noexcept
	{
		// QUEUED STATES FIRST. A pending push holds a state the client has
		// already constructed and this context has never entered - no
		// set_context, no init - and it is the most recently built thing here,
		// so it is the one most likely to borrow from a state still on the
		// stack. It dies before them, not with them.
		//
		// Not defensive padding: the queue is empty whenever a public call has
		// returned, so the one way to arrive here with something in it is a
		// drain that did not finish - an init() or an on_result that threw,
		// carrying the exception out through update() and leaving whatever was
		// issued behind it still waiting. A client unwinding out of main from
		// there destroys the Application, which lands exactly here. deferring_
		// goes back with it, so what is left is a context in its initial state
		// rather than one stuck mid-drain.
		this->pending_.clear();
		this->deferring_ = false;

		// TOP DOWN, AND EXPLICITLY NOT frames_.clear(), which destroys front to
		// back - the BOTTOM state first. The stack is ordered by dependency:
		// what a screen pushes above itself may borrow what it owns, and
		// ColourWars is exactly that shape, with every menu page above the
		// bottom state borrowing the menu context that state holds. Draining
		// bottom-first would trade one dangling reference for another and look
		// like a fix.
		while (!this->frames_.empty())
		{
			this->frames_.pop_back();
		}
	}

	void StateContext::notify_activation(bool active)
	{
		// The edge. Two messages saying the same thing is one piece of news,
		// and the second of them is not an event a state should be able to
		// hear.
		if (active == this->active_)
		{
			return;
		}

		// Before the walk, so a state asking active() from inside its own
		// callback is told what it is being told.
		this->active_ = active;

		if (this->frames_.empty())
		{
			// Not a special case. The window is up from initialize() and the
			// first state does not arrive until run(), so a message can land
			// here with nothing on the stack - and the level above is the
			// whole of what that message meant, kept for whoever arrives next.
			return;
		}

		// TOP DOWN: push's order reversed, which is clear()'s order too. What
		// is on top is what the player was last looking at, so it answers
		// first.
		//
		// Deferring across the whole walk, not per frame. A state that closes
		// itself when the player looks away would otherwise resize frames_
		// from inside the loop that is indexing it.
		this->deferring_ = true;
		for (size_t i = this->frames_.size(); i > 0; i--)
		{
			State* state = this->frames_[i - 1].state.get();
			if (active)
			{
				state->on_activated();
			}
			else
			{
				state->on_deactivated();
			}
		}
		this->deferring_ = false;

		this->apply_pending();
	}

	bool StateContext::active() const
	{
		return this->active_;
	}

	int StateContext::depth() const
	{
		// The applied stack. Operations queued during this update are not in it
		// yet, which is the same thing transition_to has always meant.
		return static_cast<int>(this->frames_.size());
	}

	void StateContext::push_frame(std::unique_ptr<State> state,
		std::function<void(const std::any&)> on_result)
	{
		if (state == nullptr)
		{
			throw std::invalid_argument("StateContext::push(nullptr).");
		}

		PendingOp op;
		op.kind = PendingOp::Kind::push;
		op.state = std::move(state);
		op.on_result = std::move(on_result);
		this->queue(std::move(op));
	}

	void StateContext::pop_frame(std::any result)
	{
		// Checked here as well as in apply_pop, because here is where the
		// mistake was written and the caller is still on the stack to say so.
		if (this->frames_.empty())
		{
			throw std::logic_error(
				"StateContext::pop with nothing on the stack.");
		}

		PendingOp op;
		op.kind = PendingOp::Kind::pop;
		op.result = std::move(result);
		this->queue(std::move(op));
	}

	void StateContext::queue(PendingOp op)
	{
		// A transition says who occupies the top frame. Saying it twice in one
		// update does not make two occupants, so the second overwrites the
		// first and the first never becomes live - it never runs init(), which
		// for a menu page is a widget tree and a sound.
		//
		// Only against another pending transition: a transition after a push is
		// about a different frame, and collapsing those would silently drop an
		// operation.
		if (op.kind == PendingOp::Kind::transition &&
			!this->pending_.empty() &&
			this->pending_.back().kind == PendingOp::Kind::transition)
		{
			this->pending_.back() = std::move(op);
		}
		else
		{
			this->pending_.push_back(std::move(op));
		}

		if (!this->deferring_)
		{
			this->apply_pending();
		}
	}

	void StateContext::apply_pending()
	{
		// Deferring through the drain itself: init() and on_resume() run in
		// here, and anything they ask for joins the back of this queue rather
		// than reentering.
		this->deferring_ = true;
		while (!this->pending_.empty())
		{
			PendingOp op = std::move(this->pending_.front());
			this->pending_.erase(this->pending_.begin());

			switch (op.kind)
			{
			case PendingOp::Kind::transition:
				this->apply_transition(std::move(op.state));
				break;
			case PendingOp::Kind::push:
				this->apply_push(std::move(op.state), std::move(op.on_result));
				break;
			case PendingOp::Kind::pop:
				this->apply_pop(op.result);
				break;
			}
		}
		this->deferring_ = false;
	}

	void StateContext::apply_transition(std::unique_ptr<State> state)
	{
		if (this->frames_.empty())
		{
			this->frames_.emplace_back();
		}

		// The frame's result callback survives the transition, and that is the
		// point of the callback living on the frame: the pause menu's
		// confirmation page replaces its initial page and then pops with the
		// answer the *pause menu* was pushed for.
		//
		// This assignment is what destroys the outgoing state, and by now it
		// has nothing of its own on the call stack.
		this->frames_.back().state = std::move(state);
		this->enter(this->frames_.back().state.get());
	}

	void StateContext::apply_push(std::unique_ptr<State> state,
		std::function<void(const std::any&)> on_result)
	{
		if (!this->frames_.empty())
		{
			this->frames_.back().state->on_suspend();
		}

		Frame frame;
		frame.state = std::move(state);
		frame.on_result = std::move(on_result);
		this->frames_.push_back(std::move(frame));

		this->enter(this->frames_.back().state.get());
	}

	void StateContext::apply_pop(const std::any& result)
	{
		if (this->frames_.empty())
		{
			throw std::logic_error(
				"StateContext::pop with nothing on the stack.");
		}

		Frame frame = std::move(this->frames_.back());
		this->frames_.pop_back();

		// Destroyed before anyone is told, so both the resumed state and the
		// callback run with it already gone.
		frame.state.reset();

		if (!this->frames_.empty())
		{
			this->frames_.back().state->on_resume();
		}

		if (frame.on_result)
		{
			frame.on_result(result);
		}
	}

	void StateContext::enter(State* state)
	{
		state->set_context(this);
		state->init();
	}
}
