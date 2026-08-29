#include "engine/core/thread_pool.h"

#include <windows.h>

#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace labrador
{
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

        // Owns the callable for the lifetime of one submission, plus the back
        // pointer the callback needs to report a failure.
        struct task_item
        {
            std::function<void()> task;
            Impl* pool = nullptr;
        };

        static void CALLBACK work_callback(PTP_CALLBACK_INSTANCE,
            PVOID parameter, PTP_WORK);

        void record_exception(std::exception_ptr exception);

        int min_num_threads = -1;
        int max_num_threads = -1;

        std::vector<PTP_WORK> work_items;
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
        SetThreadpoolThreadMaximum(pool,
            static_cast<DWORD>(maximum)); // Set maximum number of threads

        SetThreadpoolThreadMinimum(pool,
            static_cast<DWORD>(minimum)); // Set minimum number of threads

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
        CloseThreadpoolCleanupGroupMembers(cleanup_group, FALSE, nullptr);
        CloseThreadpoolCleanupGroup(cleanup_group);
        CloseThreadpool(pool);
        DestroyThreadpoolEnvironment(&callback_environment);
    }

    void ThreadPool::Impl::add_task(std::function<void()> task)
    {
        std::unique_ptr<task_item> item =
            std::make_unique<task_item>(task_item{ std::move(task), this });

        PTP_WORK work = CreateThreadpoolWork(work_callback, item.get(),
            &callback_environment);
        if (!work)
        {
            // item is still owned here, so the failure does not leak the callable.
            throw std::runtime_error("Failed to create thread pool work object.");
        }
        work_items.push_back(work);

        // The callback owns the item from the moment it is submitted.
        item.release();
        SubmitThreadpoolWork(work);
    }

    void ThreadPool::Impl::wait_for_tasks_to_complete()
    {
        for (PTP_WORK work : work_items)
        {
            WaitForThreadpoolWorkCallbacks(work, FALSE);
            CloseThreadpoolWork(work);
        }
        work_items.clear();

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
        // unique_ptr rather than a trailing delete: the task must be freed whether
        // it returns or throws.
        std::unique_ptr<task_item> item(static_cast<task_item*>(parameter));
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

    // What is left of ThreadPool is the seam: five calls that forward, and a
    // destructor that has to be here rather than defaulted in the header,
    // because unique_ptr<Impl> cannot destroy an incomplete type.
    ThreadPool::ThreadPool(int min_num_threads, int max_num_threads) :
        impl_(std::make_unique<Impl>(min_num_threads, max_num_threads))
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
