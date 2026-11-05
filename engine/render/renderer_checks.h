#pragma once

#include <cstddef>
#include <memory>
#include <vector>

// The argument checks renderer.h states for create_device, device_info,
// set_view_count and view, made once for every backend.
//
// They are the seam's contract rather than any backend's behaviour, so each
// backend's Renderer calls these before its own work and none of them writes
// the messages itself. A caller that breaks one of these rules gets the same
// exception and the same words in every configuration.

namespace labrador
{
	// Throws std::invalid_argument for a view capacity below one.
	void check_view_capacity(int view_capacity);

	// Throws std::logic_error when `created` is false: device_info has no
	// truthful answer before create_device.
	void check_device_created(bool created);

	// Throws std::out_of_range for an index outside [0, view_count).
	void check_view_index(int index, int view_count);

	// Throws std::out_of_range for a count outside [0, capacity].
	void check_view_count_in_capacity(int count, int capacity);

	// Throws std::logic_error saying that lowering the count to `count` would
	// drop view `dropped`, which has been drawn into this frame.
	[[noreturn]] void refuse_dropped_view(int count, int dropped);

	// Renderer::set_view_count's two checks. `views` holds the capacity
	// create_device made and `current_count` is the count before this call.
	// `View` is the backend's DrawList::View; each has a `touched` flag that
	// says whether anything was drawn into it this frame.
	//
	// Lowering the count past a view that was drawn into strands that
	// recording: submit would not execute it. A backend that records into a
	// command list would also show those commands at the top of the next
	// frame's list. The check refuses the call instead.
	template <typename View>
	void check_view_count(int count, int current_count,
		const std::vector<std::unique_ptr<View>>& views)
	{
		check_view_count_in_capacity(count, static_cast<int>(views.size()));

		for (int i = count; i < current_count; i++)
		{
			if (views[static_cast<std::size_t>(i)]->touched)
			{
				refuse_dropped_view(count, i);
			}
		}
	}
}
