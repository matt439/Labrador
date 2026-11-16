#include "tests/asan_allocation.h"

#if defined(LABRADOR_ASAN)

#if !defined(__SANITIZE_ADDRESS__)
#error LABRADOR_ASAN allocation probes require AddressSanitizer instrumentation.
#endif

#include <sanitizer/allocator_interface.h>
#include <Windows.h>
#include <intrin.h>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <malloc.h>
#include <new>
#include <stdexcept>

namespace
{
	struct HookSlot
	{
		volatile long claimed = 0;
		volatile long thread = 0;
		labrador_tests::AsanAllocationHooks hooks;
		bool inside_new = false;
	};

	std::array<HookSlot, 64> hook_slots;
	volatile long active_slots = 0;

	// ASan may call its allocator hooks after compiler TLS has been released.
	// A fixed table of live scopes avoids touching TLS on that teardown path.
	// Publishing a slot uses interlocked operations; only its owning thread
	// reads or writes its callbacks while that thread id is published.
	HookSlot* current_slot() noexcept
	{
		if (_InterlockedCompareExchange(&active_slots, 0, 0) == 0) return nullptr;
		const long thread = static_cast<long>(GetCurrentThreadId());
		for (HookSlot& slot : hook_slots)
		{
			if (_InterlockedCompareExchange(&slot.thread, 0, 0) == thread) return &slot;
		}
		return nullptr;
	}

	void __cdecl observe_malloc(const volatile void*, std::size_t bytes)
	{
		HookSlot* const slot = current_slot();
		if (slot != nullptr && !slot->inside_new && slot->hooks.after_malloc != nullptr)
		{
			slot->hooks.after_malloc(bytes);
		}
	}

	void __cdecl observe_free(const volatile void*)
	{
	}

	// Registration happens during executable initialization, before test
	// cases can create threads. The permanent hook has no live scope then.
	const int installed_hooks = __sanitizer_install_malloc_and_free_hooks(observe_malloc, observe_free);

	void* allocate(std::size_t bytes, std::size_t alignment)
	{
		HookSlot* const slot = current_slot();
		if (slot != nullptr && slot->hooks.before_new != nullptr && !slot->hooks.before_new(bytes))
		{
			throw std::bad_alloc();
		}
		for (;;)
		{
			// malloc remains ASan's intercepted allocator. Suppress only the
			// duplicate notification of a request already seen by before_new.
			if (slot != nullptr) slot->inside_new = slot->hooks.before_new != nullptr;
			void* const allocated = alignment == 0 ? std::malloc(bytes == 0 ? 1 : bytes) :
				_aligned_malloc(bytes == 0 ? 1 : bytes, alignment);
			if (slot != nullptr) slot->inside_new = false;
			if (allocated != nullptr)
			{
				return allocated;
			}
			const std::new_handler handler = std::get_new_handler();
			if (handler == nullptr)
			{
				throw std::bad_alloc();
			}
			handler();
		}
	}
}

namespace labrador_tests
{
	AsanAllocationHooks asan_allocation_hooks() noexcept
	{
		const HookSlot* const slot = current_slot();
		return slot == nullptr ? AsanAllocationHooks{} : slot->hooks;
	}

	void set_asan_allocation_hooks(AsanAllocationHooks hooks)
	{
		if (installed_hooks == 0)
		{
			throw std::logic_error("Could not install ASan allocation observation");
		}
		HookSlot* slot = current_slot();
		if (hooks == AsanAllocationHooks{})
		{
			if (slot != nullptr)
			{
				slot->hooks = {};
				_InterlockedExchange(&slot->thread, 0);
				_InterlockedDecrement(&active_slots);
				_InterlockedExchange(&slot->claimed, 0);
			}
			return;
		}
		if (slot == nullptr)
		{
			for (HookSlot& available : hook_slots)
			{
				if (_InterlockedCompareExchange(&available.claimed, 1, 0) == 0)
				{
					slot = &available;
					slot->hooks = hooks;
					_InterlockedExchange(&slot->thread, static_cast<long>(GetCurrentThreadId()));
					_InterlockedIncrement(&active_slots);
					return;
				}
			}
			throw std::logic_error("Too many active ASan allocation probe threads");
		}
		slot->hooks = hooks;
	}
}

// Complete standard replacement family, linked only into ASan test programs.
// Keeping both sides on intercepted malloc/free preserves ASan red zones and
// container annotations. Specific new/delete mismatch diagnostics require
// ASan's own operators and are outside this test allocator's coverage.
void* operator new(std::size_t bytes) { return allocate(bytes, 0); }
void* operator new[](std::size_t bytes) { return allocate(bytes, 0); }
void* operator new(std::size_t bytes, std::align_val_t alignment)
{
	return allocate(bytes, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t bytes, std::align_val_t alignment)
{
	return allocate(bytes, static_cast<std::size_t>(alignment));
}
void* operator new(std::size_t bytes, const std::nothrow_t&) noexcept
{
	try { return ::operator new(bytes); } catch (...) { return nullptr; }
}
void* operator new[](std::size_t bytes, const std::nothrow_t&) noexcept
{
	try { return ::operator new[](bytes); } catch (...) { return nullptr; }
}
void* operator new(std::size_t bytes, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
	try { return ::operator new(bytes, alignment); } catch (...) { return nullptr; }
}
void* operator new[](std::size_t bytes, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
	try { return ::operator new[](bytes, alignment); } catch (...) { return nullptr; }
}
void operator delete(void* value) noexcept { std::free(value); }
void operator delete[](void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }
void operator delete[](void* value, std::size_t) noexcept { std::free(value); }
void operator delete(void* value, const std::nothrow_t&) noexcept { std::free(value); }
void operator delete[](void* value, const std::nothrow_t&) noexcept { std::free(value); }
void operator delete(void* value, std::align_val_t) noexcept { _aligned_free(value); }
void operator delete[](void* value, std::align_val_t) noexcept { _aligned_free(value); }
void operator delete(void* value, std::size_t, std::align_val_t) noexcept { _aligned_free(value); }
void operator delete[](void* value, std::size_t, std::align_val_t) noexcept { _aligned_free(value); }
void operator delete(void* value, std::align_val_t, const std::nothrow_t&) noexcept { _aligned_free(value); }
void operator delete[](void* value, std::align_val_t, const std::nothrow_t&) noexcept { _aligned_free(value); }

#endif
