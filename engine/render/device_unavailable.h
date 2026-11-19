#pragma once

#include <stdexcept>
#include <string>

namespace labrador
{
	// The selected renderer cannot meet its device or context requirements at
	// startup. This does not establish that the machine physically lacks a GPU.
	// The message retains the backend's reason and any native error details.
	//
	// Catch this category to choose a startup refusal message or an optional
	// graphics policy. It remains a std::runtime_error for existing handlers.
	// Content, shaders, invalid arguments/state, unrelated API failures, audio
	// and runtime device-loss recovery are outside this category. No fallback
	// backend is selected by throwing or catching it.
	// Ambiguous browser context refusals can also be caller-owned canvas state
	// and remain ordinary runtime errors; this category is not exhaustive.
	class RenderDeviceUnavailable : public std::runtime_error
	{
	public:
		// Stores reason in the runtime_error base; what() preserves its text.
		explicit RenderDeviceUnavailable(const std::string& reason)
			: std::runtime_error(reason)
		{
		}
	};
}
