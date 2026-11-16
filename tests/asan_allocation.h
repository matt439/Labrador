#pragma once

#if defined(LABRADOR_ASAN)

#include <cstddef>

namespace labrador_tests
{
	// Test-executable hooks on the calling thread. before_new may refuse a
	// C++ allocation; after_malloc observes other successful allocations that
	// ASan manages. A C++ request is observed only before its allocator call.
	// Callbacks must not allocate or throw. The returned hooks are borrowed
	// function pointers and stay valid until their installing scope restores them.
	struct AsanAllocationHooks
	{
		bool (*before_new)(std::size_t bytes) noexcept = nullptr;
		void (*after_malloc)(std::size_t bytes) noexcept = nullptr;

		bool operator==(const AsanAllocationHooks&) const = default;
	};

	AsanAllocationHooks asan_allocation_hooks() noexcept;
	// Registers at most 64 concurrent probe threads. Throws std::logic_error
	// if the runtime hook could not be installed or the table has no free slot.
	// Empty hooks remove this thread's record; successful changes do not allocate.
	void set_asan_allocation_hooks(AsanAllocationHooks hooks);
}

#endif
