#pragma once

namespace descriptor_query_counts
{
	void gpu_query(const char* function) noexcept;
	void back_buffer_query() noexcept;
	void begin_frame() noexcept;
	void end_frame() noexcept;
}
