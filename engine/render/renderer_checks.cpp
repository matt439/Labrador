#include "engine/render/renderer_checks.h"

#include <algorithm>
#include <cmath>
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

	void check_post_chain(std::span<const PostEffect> effects,
		bool frame_begun)
	{
		if (frame_begun)
		{
			throw std::logic_error("set_post_effects was called inside a "
				"frame. The chain decides what every view draws into, which is "
				"bound as the frame's views are declared; set it before "
				"begin_frame or after end_frame.");
		}
		// One material a frame until multi-pass chains exist
		// (docs/effects.md#13).
		if (effects.size() > 1)
		{
			throw std::invalid_argument("set_post_effects was given more than "
				"one effect; this build draws a chain of at most one.");
		}
	}

	void check_post_values(const std::vector<PostEffect>& chain,
		PostEffect effect, const PostValues& values)
	{
		if (std::find(chain.begin(), chain.end(), effect) == chain.end())
		{
			throw std::invalid_argument("set_post_values was given an effect "
				"the chain does not contain. Add it with set_post_effects "
				"first.");
		}
		for (const float value : values.values)
		{
			if (!std::isfinite(value))
			{
				throw std::invalid_argument("set_post_values was given a value "
					"that is not a finite number.");
			}
		}
	}

	void check_begin_overlay(bool frame_begun, int overlay_start,
		int view_count, int capacity)
	{
		if (!frame_begun)
		{
			throw std::logic_error("begin_overlay was called outside a frame. "
				"Call it between begin_frame and submit.");
		}
		if (overlay_start >= 0)
		{
			throw std::logic_error("begin_overlay was called twice in one frame. "
				"Every view declared after the first call is already an overlay "
				"view.");
		}
		if (view_count >= capacity)
		{
			throw std::out_of_range("begin_overlay needs one more view than "
				"the frame has declared, and the capacity given to "
				"create_device has none to spare. Raise the view capacity by "
				"one.");
		}
	}

	void refuse_post_chain_on(const char* backend,
		std::span<const PostEffect> effects)
	{
		if (!effects.empty())
		{
			throw std::logic_error(std::string("The ") + backend +
				" renderer has no post stage yet, so set_post_effects accepts "
				"only an empty chain here. Build the d3d11 or null backend for "
				"post effects.");
		}
	}
}
