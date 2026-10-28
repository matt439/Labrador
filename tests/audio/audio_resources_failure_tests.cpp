#if defined(_MSC_VER) && defined(_DEBUG) && defined(_DLL) && !defined(__EMSCRIPTEN__)

#include <doctest/doctest.h>

#include "engine/audio/audio_resources.h"

#include <crtdbg.h>

#include <atomic>
#include <cstddef>
#include <new>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace
{
	struct AllocationFailureSample
	{
		std::size_t fail_on = 1;
		std::size_t attempts = 0;
		bool injected = false;
		bool hook_conflict = false;
	};

	thread_local AllocationFailureSample* active_sample = nullptr;
	std::atomic<_CRT_ALLOC_HOOK> previous_hook = nullptr;
	static_assert(std::atomic<_CRT_ALLOC_HOOK>::is_always_lock_free);
	std::atomic_flag hook_owned = ATOMIC_FLAG_INIT;

	int __cdecl refuse_allocation(int operation, void* data, size_t bytes,
		int block_type, long request, const unsigned char* file, int line) noexcept
	{
		const _CRT_ALLOC_HOOK previous = previous_hook.load(std::memory_order_relaxed);
		if (previous != nullptr &&
			previous(operation, data, bytes, block_type, request, file, line) == 0)
		{
			return 0;
		}

		if (active_sample != nullptr && _BLOCK_TYPE(block_type) == _NORMAL_BLOCK &&
			(operation == _HOOK_ALLOC || operation == _HOOK_REALLOC))
		{
			++active_sample->attempts;
			if (active_sample->attempts == active_sample->fail_on)
			{
				active_sample->injected = true;
				// The CRT warning and exception machinery must not encounter a
				// second refusal while reporting or unwinding this one.
				active_sample = nullptr;
				return 0;
			}
		}
		return 1;
	}

	// One process-wide hook owner, one targeted thread, one refusal. No heap
	// operation or assertion belongs in the hook. Other instrumentation must
	// not replace the hook or warning mode concurrently; conflicts fail the
	// post-scope checks instead of silently overwriting the new owner.
	class AllocationFailureScope
	{
	public:
		explicit AllocationFailureScope(AllocationFailureSample& sample) : sample_(sample)
		{
			if (hook_owned.test_and_set(std::memory_order_acquire))
			{
				throw std::logic_error("Audio allocation probes cannot overlap");
			}
			try
			{
				const int expected_mode = _CrtSetReportMode(_CRT_WARN, _CRTDBG_REPORT_MODE);
				if (expected_mode < 0)
				{
					throw std::logic_error("Could not read the CRT warning mode");
				}
				this->warning_mode_ = expected_mode & ~_CRTDBG_MODE_WNDW;
				this->previous_warning_mode_ = _CrtSetReportMode(_CRT_WARN, this->warning_mode_);
				this->warning_mode_installed_ = true;
				if (this->previous_warning_mode_ != expected_mode)
				{
					throw std::logic_error("CRT warning mode changed concurrently");
				}

				const _CRT_ALLOC_HOOK expected = _CrtGetAllocHook();
				if (expected == refuse_allocation)
				{
					throw std::logic_error("Unexpected audio allocation probe hook");
				}
				previous_hook.store(expected, std::memory_order_relaxed);
				this->previous_ = _CrtSetAllocHook(refuse_allocation);
				this->hook_installed_ = true;
				if (this->previous_ != expected)
				{
					throw std::logic_error("CRT allocation hook changed concurrently");
				}
				active_sample = &sample;
			}
			catch (...)
			{
				this->restore();
				hook_owned.clear(std::memory_order_release);
				throw;
			}
		}

		~AllocationFailureScope()
		{
			this->restore();
			hook_owned.clear(std::memory_order_release);
		}

		AllocationFailureScope(const AllocationFailureScope&) = delete;
		AllocationFailureScope& operator=(const AllocationFailureScope&) = delete;

	private:
		void restore() noexcept
		{
			active_sample = nullptr;
			if (this->hook_installed_)
			{
				if (_CrtGetAllocHook() == refuse_allocation)
				{
					_CrtSetAllocHook(this->previous_);
				}
				else
				{
					this->sample_.hook_conflict = true;
				}
			}
			if (this->warning_mode_installed_)
			{
				if (_CrtSetReportMode(_CRT_WARN, _CRTDBG_REPORT_MODE) == this->warning_mode_)
				{
					_CrtSetReportMode(_CRT_WARN, this->previous_warning_mode_);
				}
				else
				{
					this->sample_.hook_conflict = true;
				}
			}
		}

		AllocationFailureSample& sample_;
		_CRT_ALLOC_HOOK previous_ = nullptr;
		int previous_warning_mode_ = 0;
		int warning_mode_ = 0;
		bool hook_installed_ = false;
		bool warning_mode_installed_ = false;
	};
}

TEST_SUITE("AudioResourcesFailureTests")
{
	TEST_CASE("sound-group allocation probe calibrates one-shot refusal and restoration")
	{
		REQUIRE(std::get_new_handler() == nullptr);
		std::vector<unsigned char> first;
		std::vector<unsigned char> after_refusal;
		std::vector<unsigned char> unwinding;
		const _CRT_ALLOC_HOOK initial_hook = _CrtGetAllocHook();
		const int initial_mode = _CrtSetReportMode(_CRT_WARN, _CRTDBG_REPORT_MODE);
		AllocationFailureSample sample;
		bool refused = false;
		{
			AllocationFailureScope scope(sample);
			try
			{
				first.resize(32, 7);
			}
			catch (const std::bad_alloc&)
			{
				refused = true;
			}
			// Still inside the scope, but the one-shot has disarmed itself.
			// The vectors were constructed before arming and are destroyed after.
			after_refusal.resize(32, 9);
		}
		REQUIRE_FALSE(sample.hook_conflict);
		REQUIRE(_CrtGetAllocHook() == initial_hook);
		REQUIRE(_CrtSetReportMode(_CRT_WARN, _CRTDBG_REPORT_MODE) == initial_mode);
		REQUIRE(sample.injected);
		REQUIRE(refused);
		CHECK(sample.attempts == 1);
		CHECK(first.empty());
		REQUIRE(after_refusal.size() == 32);
		CHECK(after_refusal[9] == 9);

		AllocationFailureSample unwind_sample;
		bool unwind_refused = false;
		try
		{
			AllocationFailureScope scope(unwind_sample);
			unwinding.resize(32, 5);
		}
		catch (const std::bad_alloc&)
		{
			unwind_refused = true;
		}
		REQUIRE_FALSE(unwind_sample.hook_conflict);
		REQUIRE(_CrtGetAllocHook() == initial_hook);
		REQUIRE(_CrtSetReportMode(_CRT_WARN, _CRTDBG_REPORT_MODE) == initial_mode);
		REQUIRE(unwind_sample.injected);
		REQUIRE(unwind_refused);
		CHECK(unwind_sample.attempts == 1);
		CHECK(unwinding.empty());
	}

	TEST_CASE("failed sound-group additions stay absent and retryable")
	{
		REQUIRE(std::get_new_handler() == nullptr);
		constexpr std::size_t failpoint_limit = 64;
		std::size_t failures = 0;
		bool reached_success = false;
		for (std::size_t fail_on = 1; fail_on <= failpoint_limit; ++fail_on)
		{
			labrador::AudioResources resources;
			const std::string kept_name = "kept";
			const std::string attempted_name = "retry";
			const std::string later_name = "later";
			const labrador::SoundGroupHandle kept = resources.add_sound_group(kept_name);
			resources.set_group_volume(kept, 0.25f);
			const _CRT_ALLOC_HOOK initial_hook = _CrtGetAllocHook();
			const int initial_mode = _CrtSetReportMode(_CRT_WARN, _CRTDBG_REPORT_MODE);
			AllocationFailureSample sample;
			sample.fail_on = fail_on;
			labrador::SoundGroupHandle attempted;
			bool refused = false;
			try
			{
				AllocationFailureScope scope(sample);
				attempted = resources.add_sound_group(attempted_name);
			}
			catch (const std::bad_alloc&)
			{
				refused = true;
			}

			CAPTURE(fail_on);
			CAPTURE(sample.attempts);
			REQUIRE_FALSE(sample.hook_conflict);
			REQUIRE(_CrtGetAllocHook() == initial_hook);
			REQUIRE(_CrtSetReportMode(_CRT_WARN, _CRTDBG_REPORT_MODE) == initial_mode);
			CHECK(resources.resolve_sound_group(kept_name) == kept);
			CHECK(resources.group_volume(kept) == doctest::Approx(0.25f));
			if (!refused)
			{
				REQUIRE_FALSE(sample.injected);
				CHECK(sample.attempts < fail_on);
				CHECK(resources.resolve_sound_group(attempted_name) == attempted);
				CHECK(attempted != kept);
				CHECK(resources.group_volume(attempted) == doctest::Approx(1.0f));
				reached_success = true;
				break;
			}

			REQUIRE(sample.injected);
			CHECK(sample.attempts == fail_on);
			++failures;
			CHECK_THROWS_AS(std::ignore = resources.resolve_sound_group(attempted_name),
				std::out_of_range);
			const labrador::SoundGroupHandle later = resources.add_sound_group(later_name);
			CHECK(later != kept);
			CHECK_THROWS_AS(std::ignore = resources.resolve_sound_group(attempted_name),
				std::out_of_range);
			labrador::SoundGroupHandle retried;
			REQUIRE_NOTHROW(retried = resources.add_sound_group(attempted_name));
			CHECK(retried != later);
			CHECK(retried != kept);
			CHECK(resources.resolve_sound_group(attempted_name) == retried);
			CHECK(resources.group_volume(retried) == doctest::Approx(1.0f));
			resources.set_group_volume(later, 0.5f);
			resources.set_group_volume(retried, 0.75f);
			CHECK(resources.group_volume(kept) == doctest::Approx(0.25f));
			CHECK(resources.group_volume(later) == doctest::Approx(0.5f));
			CHECK(resources.group_volume(retried) == doctest::Approx(0.75f));
			CHECK(resources.resolve_sound_group(later_name) == later);
		}
		REQUIRE(reached_success);
		// The fixture must reach more than one allocation boundary, including
		// storage growth, instead of passing by refusing only map publication.
		CHECK(failures >= 2);
	}
}

#endif
