#include <doctest/doctest.h>
#include "engine/core/thread_pool.h"
#include <atomic>
#include <stdexcept>
#include <string>
using namespace labrador;

// What a pool promises the one thread that owns it.
//
// The platform types live behind thread_pool.h's Impl, so these cases are
// written against the header's contract alone and hold for every
// implementation behind it.
//
// NOTHING HERE ASSERTS ON PARALLELISM, deliberately. A pool with a maximum of
// four threads is allowed to run four tasks on one thread, and a test that
// demanded otherwise would fail on a machine that decided differently. What is
// contractual is that every submitted task has finished when the wait returns,
// and that a task's exception comes back to the waiter rather than killing the
// process - so those are what is asserted, and both are deterministic.

TEST_SUITE("ThreadPoolTests")
{
	TEST_CASE("the wait does not return until every task has finished")
	{
		ThreadPool pool(1, 4);
		std::atomic<int> done{ 0 };

		// More tasks than the maximum, so at least one of them has to wait
		// for a thread and the wait has to cover it.
		for (int i = 0; i < 32; ++i)
		{
			pool.add_task([&done]() { ++done; });
		}
		pool.wait_for_tasks_to_complete();

		CHECK(done.load() == 32);
	}

	TEST_CASE("a pool is reusable across waits")
	{
		ThreadPool pool(1, 2);
		std::atomic<int> done{ 0 };

		pool.add_task([&done]() { ++done; });
		pool.wait_for_tasks_to_complete();
		CHECK(done.load() == 1);

		// The submitted-work list is cleared by the wait, so a second round
		// waits for its own tasks and not for the closed handles of the
		// first.
		pool.add_task([&done]() { ++done; });
		pool.add_task([&done]() { ++done; });
		pool.wait_for_tasks_to_complete();
		CHECK(done.load() == 3);
	}

	TEST_CASE("a task's exception is rethrown on the thread that waited")
	{
		ThreadPool pool(1, 2);

		pool.add_task([]()
			{
				throw std::runtime_error("task failed");
			});

		// Not "something threw": the exception a worker caught is the one
		// the waiter gets, message included. An exception escaping a Win32
		// thread-pool callback terminates the process with no diagnostic,
		// so this is the whole of why the callback catches at all.
		bool caught = false;
		try
		{
			pool.wait_for_tasks_to_complete();
		}
		catch (const std::runtime_error& error)
		{
			caught = true;
			CHECK(std::string(error.what()) == "task failed");
		}
		CHECK(caught);
	}

	TEST_CASE("a task that throws does not stop the others running")
	{
		ThreadPool pool(1, 4);
		std::atomic<int> done{ 0 };

		pool.add_task([&done]() { ++done; });
		pool.add_task([]() { throw std::runtime_error("task failed"); });
		pool.add_task([&done]() { ++done; });
		pool.add_task([&done]() { ++done; });

		CHECK_THROWS_AS(pool.wait_for_tasks_to_complete(),
			std::runtime_error);

		// The wait closes every work item before it looks at the failure,
		// so the three that did not throw have all finished by the time it
		// throws rather than being abandoned mid-flight.
		CHECK(done.load() == 3);
	}

	TEST_CASE("a failure is reported once and does not haunt the next wait")
	{
		ThreadPool pool(1, 2);

		pool.add_task([]() { throw std::runtime_error("task failed"); });
		CHECK_THROWS_AS(pool.wait_for_tasks_to_complete(),
			std::runtime_error);

		// The recorded exception is taken rather than copied out, so a
		// pool that has reported a failure is a working pool again. A
		// second wait re-throwing the first frame's failure would blame
		// the wrong frame for it.
		std::atomic<int> done{ 0 };
		pool.add_task([&done]() { ++done; });
		CHECK_NOTHROW(pool.wait_for_tasks_to_complete());
		CHECK(done.load() == 1);
	}

	TEST_CASE("a wait with nothing submitted returns")
	{
		ThreadPool pool(1, 2);

		// The wait's contract is that every submitted task has finished,
		// which holds trivially when none was submitted.
		CHECK_NOTHROW(pool.wait_for_tasks_to_complete());
	}

	TEST_CASE("the thread counts are the ones the pool was asked for")
	{
		// Scene::draw reads max_num_threads to decide how many slices to
		// cut the view list into, so these are load-bearing rather than
		// bookkeeping.
		const ThreadPool pool(2, 6);

		CHECK(pool.min_num_threads() == 2);
		CHECK(pool.max_num_threads() == 6);
	}

	TEST_CASE("thread counts outside 1 <= minimum <= maximum are refused")
	{
		SUBCASE("a zero minimum")
		{
			CHECK_THROWS_AS(ThreadPool(0, 0), std::invalid_argument);
			CHECK_THROWS_AS(ThreadPool(0, 2), std::invalid_argument);
		}
		SUBCASE("a negative minimum")
		{
			CHECK_THROWS_AS(ThreadPool(-1, 2), std::invalid_argument);
		}
		SUBCASE("a minimum above the maximum")
		{
			CHECK_THROWS_AS(ThreadPool(4, 2), std::invalid_argument);
		}
		SUBCASE("the message states both numbers")
		{
			try
			{
				const ThreadPool pool(4, 2);
				CHECK(pool.max_num_threads() == 2);
				FAIL("a floor above the ceiling must throw");
			}
			catch (const std::invalid_argument& error)
			{
				const std::string message = error.what();
				CHECK(message.find("4 and 2") != std::string::npos);
			}
		}
	}

	TEST_CASE("equal minimum and maximum are a valid pool")
	{
		const ThreadPool pool(1, 1);

		CHECK(pool.min_num_threads() == 1);
		CHECK(pool.max_num_threads() == 1);
	}
}
