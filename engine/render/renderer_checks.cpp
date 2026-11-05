#include "engine/render/renderer_checks.h"

#include <stdexcept>
#include <string>

namespace labrador
{
	void check_view_capacity(int view_capacity)
	{
		if (view_capacity < 1)
		{
			throw std::invalid_argument(
				"Renderer::create_device needs a view capacity of at least 1.");
		}
	}

	void check_device_created(bool created)
	{
		if (!created)
		{
			throw std::logic_error(
				"Renderer::device_info requires create_device.");
		}
	}

	void check_view_index(int index, int view_count)
	{
		if (index < 0 || index >= view_count)
		{
			throw std::out_of_range("Renderer::view(" + std::to_string(index) +
				") - this frame has " + std::to_string(view_count) +
				" views.");
		}
	}

	void check_view_count_in_capacity(int count, int capacity)
	{
		if (count < 0 || count > capacity)
		{
			throw std::out_of_range("Renderer::set_view_count(" +
				std::to_string(count) + ") is outside the capacity of " +
				std::to_string(capacity) + " create_device was given.");
		}
	}

	void refuse_dropped_view(int count, int dropped)
	{
		throw std::logic_error("Renderer::set_view_count(" +
			std::to_string(count) + ") would drop view " +
			std::to_string(dropped) + ", which has already been drawn into "
			"this frame.");
	}
}
