#ifndef STATECONTEXT_H
#define STATECONTEXT_H

#include "engine/core/state.h"
#include <memory>

class StateContext
{
public:
	virtual ~StateContext() = default;
	StateContext() = default;
	void update() const;
	void draw() const;
	void transition_to(std::unique_ptr<State> state);
private:
	std::unique_ptr<State> _state = nullptr;
};

#endif // !STATECONTEXTGENERIC_H
