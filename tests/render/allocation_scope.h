#pragma once

// Counts the Debug CRT's normal-block allocation attempts on one thread, for
// the backends' warmed-submit tests, and can refuse one chosen attempt for
// their allocation-failure regressions. Only a Debug build against the DLL
// CRT has the hook; elsewhere this header declares nothing, and the tests
// that use it compile out with it.
//
// It cannot observe every driver or system heap, and it says nothing about
// time.

#if defined(_MSC_VER) && defined(_DEBUG) && defined(_DLL)

#include <doctest/doctest.h>

#include <crtdbg.h>

#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace render_tests
{
	struct AllocationSample
	{
		size_t attempts = 0;
		size_t bytes = 0;
		bool hook_conflict = false;

		// Optional. Attempts of exactly `watched_bytes` are counted, and the
		// last one's 1-based ordinal among all attempts is kept, so a
		// successful run can locate one allocation in a repeatable sequence.
		size_t watched_bytes = 0;
		size_t watched_attempts = 0;
		size_t watched_attempt = 0;

		// Optional. The attempt with this 1-based ordinal is refused, so its
		// operator new throws std::bad_alloc; zero refuses nothing.
		size_t refused_attempt = 0;
		size_t refused = 0;
		size_t refused_bytes = 0;
	};

	inline thread_local AllocationSample* active_sample = nullptr;
	inline std::atomic<_CRT_ALLOC_HOOK> previous_hook = nullptr;
	static_assert(std::atomic<_CRT_ALLOC_HOOK>::is_always_lock_free);
	inline std::atomic_flag hook_owned = ATOMIC_FLAG_INIT;

	inline int __cdecl observe_allocation(int operation, void* data, size_t bytes,
		int block_type, long request, const unsigned char* file, int line)
	{
		if (active_sample != nullptr && _BLOCK_TYPE(block_type) == _NORMAL_BLOCK &&
			(operation == _HOOK_ALLOC || operation == _HOOK_REALLOC))
		{
			AllocationSample& sample = *active_sample;
			++sample.attempts;
			sample.bytes += bytes;
			if (sample.watched_bytes != 0 && bytes == sample.watched_bytes)
			{
				++sample.watched_attempts;
				sample.watched_attempt = sample.attempts;
			}
			if (sample.attempts == sample.refused_attempt)
			{
				++sample.refused;
				sample.refused_bytes = bytes;
				return 0;
			}
		}
		const _CRT_ALLOC_HOOK previous = previous_hook.load(std::memory_order_relaxed);
		return previous == nullptr ? 1 : previous(operation, data, bytes,
			block_type, request, file, line);
	}

	// Counts into `sample` while it lives. The thread-local pointer limits
	// what is counted, not who owns the process-wide hook: these scopes
	// serialize their installations and keep any prior hook, which must
	// obey the CRT's rule against recursive heap operations. Other
	// instrumentation must not replace the hook concurrently.
	class AllocationScope
	{
	public:
		explicit AllocationScope(AllocationSample& sample) : sample_(sample)
		{
			if (hook_owned.test_and_set(std::memory_order_acquire))
			{
				throw std::logic_error("Allocation probes cannot overlap");
			}
			try
			{
				this->previous_ = _CrtGetAllocHook();
				if (this->previous_ == observe_allocation)
				{
					throw std::logic_error("Unexpected allocation probe hook");
				}
				previous_hook.store(this->previous_, std::memory_order_relaxed);
				const _CRT_ALLOC_HOOK replaced = _CrtSetAllocHook(observe_allocation);
				if (replaced != this->previous_)
				{
					if (_CrtGetAllocHook() == observe_allocation)
					{
						_CrtSetAllocHook(replaced);
					}
					throw std::logic_error("Allocation hook changed concurrently");
				}
				active_sample = &sample;
			}
			catch (...)
			{
				hook_owned.clear(std::memory_order_release);
				throw;
			}
		}

		~AllocationScope()
		{
			active_sample = nullptr;
			if (_CrtGetAllocHook() == observe_allocation)
			{
				_CrtSetAllocHook(this->previous_);
			}
			else
			{
				this->sample_.hook_conflict = true;
			}
			hook_owned.clear(std::memory_order_release);
		}

		AllocationScope(const AllocationScope&) = delete;
		AllocationScope& operator=(const AllocationScope&) = delete;

	private:
		AllocationSample& sample_;
		_CRT_ALLOC_HOOK previous_ = nullptr;
	};

	// Proves the probe sees an allocation it should, and leaves the hook as
	// it found it, before a test trusts it to report zero.
	inline void require_allocation_scope_counts()
	{
		const _CRT_ALLOC_HOOK initial = _CrtGetAllocHook();
		AllocationSample calibration;
		size_t control_value = 0;
		{
			AllocationScope scope(calibration);
			const std::vector<size_t> control(32, 7);
			control_value = control[9];
		}
		REQUIRE_FALSE(calibration.hook_conflict);
		REQUIRE(_CrtGetAllocHook() == initial);
		REQUIRE(calibration.attempts > 0);
		REQUIRE(calibration.bytes >= 32 * sizeof(size_t));
		REQUIRE(control_value == 7);
	}
}

#endif
