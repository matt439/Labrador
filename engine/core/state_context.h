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
		void update() const;
		void draw() const;
		void transition_to(std::unique_ptr<State> state);
	private:
		std::unique_ptr<State> state_ = nullptr;
	};
}
