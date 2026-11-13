#include "engine/core/thread_pool.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace labrador
{
	namespace
	{
		// Checked once here, before either Impl sees the numbers: a negative
		// count cast to DWORD asks Windows for billions of threads, and a
		// floor above the ceiling is a pool nobody can describe.
		int checked_minimum(int minimum, int maximum)
		{
			if (minimum < 1 || maximum < minimum)
			{
				throw std::invalid_argument("ThreadPool needs 1 <= min_num_threads "
					"<= max_num_threads; it was asked for " +
					std::to_string(minimum) + " and " + std::to_string(maximum) + ".");
			}
			return minimum;
		}
	}

#if defined(_WIN32)
	// The Win32 thread pool, and the whole of what this class knows about
	// Windows. Every member below is here rather than on ThreadPool itself,
	// which is what keeps <windows.h> out of the header (thread_pool.h).
	//
	// Public and bare, in the shape engine/render/<backend>/backend.h gives an
	// Impl: it is declared in a header nobody else can reach and defined in
	// the one file that uses it, so an access specifier here would be
	// protecting the class from itself.
	class ThreadPool::Impl
	{
	public:
		Impl(int minimum, int maximum);
		~Impl();

		Impl(const Impl&) = delete;
		Impl& operator=(const Impl&) = delete;

		void add_task(std::function<void()> task);
		void wait_for_tasks_to_complete();

		// One submission slot: the callable it is running, the back pointer
		// the callback needs to report a failure, and the work object that
		// runs it. Slots outlive their submissions and are reused in order,
		// so once the pool has held as many tasks between two waits as it is
		// given now, add_task builds nothing - which is what keeps a warmed
		// Scene::draw allocation-free (PHILOSOPHY, Performance).
		struct TaskItem
		{
			std::function<void()> task;
			Impl* pool = nullptr;
			PTP_WORK work = nullptr;
		};

		static void CALLBACK work_callback(PTP_CALLBACK_INSTANCE,
			PVOID parameter, PTP_WORK);

		void record_exception(std::exception_ptr exception);

		int min_num_threads = -1;
		int max_num_threads = -1;

		// Every slot built so far; the first `submitted` hold this wait's
		// tasks. Each item's address is its work object's context, so the
		// items are held by pointer and never move.
		std::vector<std::unique_ptr<TaskItem>> items;
		size_t submitted = 0;
		TP_CALLBACK_ENVIRON callback_environment = {};
		PTP_POOL pool = nullptr;
		PTP_CLEANUP_GROUP cleanup_group = nullptr;

		std::mutex exception_mutex;
		std::exception_ptr first_exception;
	};

	ThreadPool::Impl::Impl(int minimum, int maximum) :
		min_num_threads(minimum),
		max_num_threads(maximum)
	{
		InitializeThreadpoolEnvironment(&callback_environment);
		pool = CreateThreadpool(nullptr);
		if (!pool)
		{
			throw std::runtime_error("Failed to create thread pool.");
		}
		SetThreadpoolThreadMaximum(pool, static_cast<DWORD>(maximum));
		if (!SetThreadpoolThreadMinimum(pool, static_cast<DWORD>(minimum)))
		{
			CloseThreadpool(pool);
			throw std::runtime_error("Failed to start the thread pool's minimum of " +
				std::to_string(minimum) + " threads.");
		}

		cleanup_group = CreateThreadpoolCleanupGroup();
		if (!cleanup_group)
		{
			CloseThreadpool(pool);
			throw std::runtime_error("Failed to create cleanup group.");
		}
		SetThreadpoolCallbackPool(&callback_environment, pool);
		SetThreadpoolCallbackCleanupGroup(&callback_environment, cleanup_group, nullptr);
	}

	ThreadPool::Impl::~Impl()
	{
		// Waits for any callback still running and closes every slot's work
		// object; the slots themselves are freed after this body, with items.
		CloseThreadpoolCleanupGroupMembers(cleanup_group, FALSE, nullptr);
		CloseThreadpoolCleanupGroup(cleanup_group);
		CloseThreadpool(pool);
		DestroyThreadpoolEnvironment(&callback_environment);
	}

	void ThreadPool::Impl::add_task(std::function<void()> task)
	{
		if (submitted == items.size())
		{
			// Room first, so the push below cannot throw and strand a work
			// object; a failure before it leaves the pool as it was.
			items.reserve(items.size() + 1);
			std::unique_ptr<TaskItem> item = std::make_unique<TaskItem>();
			item->pool = this;
			item->work = CreateThreadpoolWork(work_callback, item.get(),
				&callback_environment);
			if (!item->work)
			{
				throw std::runtime_error("Failed to create thread pool work object.");
			}
			items.push_back(std::move(item));
		}

		TaskItem& item = *items[submitted];
		item.task = std::move(task);
		++submitted;
		SubmitThreadpoolWork(item.work);
	}

	void ThreadPool::Impl::wait_for_tasks_to_complete()
	{
		for (size_t index = 0; index < submitted; ++index)
		{
			TaskItem& item = *items[index];
			WaitForThreadpoolWorkCallbacks(item.work, FALSE);
			// The callable's captures are released here, as they would be by
			// a pool that freed each task after running it.
			item.task = nullptr;
		}
		submitted = 0;

		std::exception_ptr failure;
		{
			std::lock_guard<std::mutex> lock(exception_mutex);
			failure = first_exception;
			first_exception = nullptr;
		}
		if (failure)
		{
			std::rethrow_exception(failure);
		}
	}

	void CALLBACK ThreadPool::Impl::work_callback(PTP_CALLBACK_INSTANCE,
		PVOID parameter, PTP_WORK)
	{
		// The slot belongs to the pool, which releases the callable at the
		// wait; the callback only runs it.
		TaskItem* item = static_cast<TaskItem*>(parameter);
		try
		{
			item->task();
		}
		catch (...)
		{
			item->pool->record_exception(std::current_exception());
		}
	}

	void ThreadPool::Impl::record_exception(std::exception_ptr exception)
	{
		std::lock_guard<std::mutex> lock(exception_mutex);
		if (!first_exception)
		{
			first_exception = exception;
		}
	}

#else
	// No threads: every task runs inside add_task, on the thread that owns
	// the pool, and the wait has only to report what happened.
	//
	// THIS IS EVERY PROMISE thread_pool.h MAKES, NOT A REDUCED SET OF THEM. A
	// pool is allowed to run its tasks on one thread, every task has finished
	// when the wait returns, and the first exception comes back from the wait
	// and not from add_task - so a task that throws is caught here exactly as
	// a worker would catch it, and the tasks after it still run. Nothing a
	// caller can observe tells this from a pool that happened to schedule
	// everything onto one worker.
	//
	// It is what a build with no thread support gets: WebAssembly without
	// pthreads, which docs/port/web.md chooses for the host page's sake
	// rather than settles for. max_num_threads still reports what the pool
	// was asked for, because Scene::draw slices its view list by it and the
	// slices are as correct run in sequence as in parallel.
	class ThreadPool::Impl
	{
	public:
		Impl(int minimum, int maximum) :
			min_num_threads(minimum),
			max_num_threads(maximum)
		{
		}

		void add_task(std::function<void()> task)
		{
			try
			{
				task();
			}
			catch (...)
			{
				if (!first_exception)
				{
					first_exception = std::current_exception();
				}
			}
		}

		void wait_for_tasks_to_complete()
		{
			std::exception_ptr failure = first_exception;
			first_exception = nullptr;
			if (failure)
			{
				std::rethrow_exception(failure);
			}
		}

		int min_num_threads = -1;
		int max_num_threads = -1;
		std::exception_ptr first_exception;
	};
#endif

	// What is left of ThreadPool is the seam: five calls that forward, and a
	// destructor that has to be here rather than defaulted in the header,
	// because unique_ptr<Impl> cannot destroy an incomplete type.
	ThreadPool::ThreadPool(int min_num_threads, int max_num_threads) :
		impl_(std::make_unique<Impl>(
			checked_minimum(min_num_threads, max_num_threads), max_num_threads))
	{
	}

	ThreadPool::~ThreadPool() = default;

	void ThreadPool::add_task(std::function<void()> task)
	{
		this->impl_->add_task(std::move(task));
	}

	void ThreadPool::wait_for_tasks_to_complete()
	{
		this->impl_->wait_for_tasks_to_complete();
	}

	int ThreadPool::min_num_threads() const
	{
		return this->impl_->min_num_threads;
	}

	int ThreadPool::max_num_threads() const
	{
		return this->impl_->max_num_threads;
	}
}
