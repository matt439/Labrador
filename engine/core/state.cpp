#include "engine/core/state.h"

void State::set_context(StateContext* context)
{
	this->context_ = context;
}
StateContext* State::get_context() const
{
	return this->context_;
}
