#include "engine/core/thread_pool.h"

ThreadPool::ThreadPool(int min_num_threads, int max_num_threads) :
    _min_num_threads(min_num_threads),
    _max_num_threads(max_num_threads)
{
    InitializeThreadpoolEnvironment(&_callback_environment);
    _pool = CreateThreadpool(nullptr);
    if (!_pool)
    {
        throw std::runtime_error("Failed to create thread pool.");
    }
    SetThreadpoolThreadMaximum(_pool,
        static_cast<DWORD>(max_num_threads)); // Set maximum number of threads

    SetThreadpoolThreadMinimum(_pool,
        static_cast<DWORD>(min_num_threads)); // Set minimum number of threads

    _cleanup_group = CreateThreadpoolCleanupGroup();
    if (!_cleanup_group)
    {
        CloseThreadpool(_pool);
        throw std::runtime_error("Failed to create cleanup group.");
    }
    SetThreadpoolCallbackPool(&_callback_environment, _pool);
    SetThreadpoolCallbackCleanupGroup(&_callback_environment, _cleanup_group, nullptr);
}

ThreadPool::~ThreadPool()
{
    CloseThreadpoolCleanupGroupMembers(_cleanup_group, FALSE, nullptr);
    CloseThreadpoolCleanupGroup(_cleanup_group);
    CloseThreadpool(_pool);
    DestroyThreadpoolEnvironment(&_callback_environment);
}

void ThreadPool::add_task(std::function<void()> task)
{
    auto item = std::make_unique<task_item>(task_item{ std::move(task), this });

    PTP_WORK work = CreateThreadpoolWork(work_callback, item.get(),
        &_callback_environment);
    if (!work)
    {
        // item is still owned here, so the failure does not leak the callable.
        throw std::runtime_error("Failed to create thread pool work object.");
    }
    _work_items.push_back(work);

    // The callback owns the item from the moment it is submitted.
    item.release();
    SubmitThreadpoolWork(work);
}

void ThreadPool::wait_for_tasks_to_complete()
{
    for (PTP_WORK work : _work_items)
    {
        WaitForThreadpoolWorkCallbacks(work, FALSE);
        CloseThreadpoolWork(work);
    }
    _work_items.clear();

    std::exception_ptr failure;
    {
        std::lock_guard<std::mutex> lock(_exception_mutex);
        failure = _first_exception;
        _first_exception = nullptr;
    }
    if (failure)
    {
        std::rethrow_exception(failure);
    }
}

void CALLBACK ThreadPool::work_callback(PTP_CALLBACK_INSTANCE,
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

void ThreadPool::record_exception(std::exception_ptr exception)
{
    std::lock_guard<std::mutex> lock(_exception_mutex);
    if (!_first_exception)
    {
        _first_exception = exception;
    }
}

int ThreadPool::get_min_num_threads() const
{
    return this->_min_num_threads;
}

int ThreadPool::get_max_num_threads() const
{
    return this->_max_num_threads;
}