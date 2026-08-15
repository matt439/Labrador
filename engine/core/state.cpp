#include "engine/core/state.h"

namespace labrador
{
	void State::set_context(StateContext* context)
	{
		this->context_ = context;
	}
	StateContext* State::context() const
	{
		return this->context_;
	}
}
