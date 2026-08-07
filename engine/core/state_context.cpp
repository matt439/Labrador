#include "engine/core/state_context.h"

namespace artattack
{
	void StateContext::update() const
	{
		if (this->state_ == nullptr)
		{
			return;
		}
		this->state_->update();
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
		this->state_ = std::move(state);
		this->state_->set_context(this);
		this->state_->init();
	}
}