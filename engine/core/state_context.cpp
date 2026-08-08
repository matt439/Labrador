#include "engine/core/state_context.h"

namespace artattack
{
	void StateContext::update()
	{
		if (this->state_ == nullptr)
		{
			return;
		}

		this->deferring_ = true;
		this->state_->update();
		this->deferring_ = false;

		// After update() returns, so the outgoing state is destroyed with
		// nothing of its own on the stack.
		this->apply_pending();
	}
	void StateContext::draw() const
	{
		if (this->state_ == nullptr)
		{
			return;
		}
		this->state_->draw();
	}
	void StateContext::transition_to(std::unique_ptr<State> state)
	{
		this->pending_ = std::move(state);
		if (!this->deferring_)
		{
			this->apply_pending();
		}
	}
	void StateContext::apply_pending()
	{
		// The loop, not an if: init() may transition again, and that
		// transition parks rather than destroying the state whose init() is
		// on the stack. The same hazard as update(), one level down.
		this->deferring_ = true;
		while (this->pending_ != nullptr)
		{
			this->state_ = std::move(this->pending_);
			this->state_->set_context(this);
			this->state_->init();
		}
		this->deferring_ = false;
	}
}
