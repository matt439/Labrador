#include "engine/core/thread_pool.h"

ThreadPool::ThreadPool(int min_num_threads, int max_num_threads) :
    min_num_threads_(min_num_threads),
    max_num_threads_(max_num_threads)
{
    InitializeThreadpoolEnvironment(&callback_environment_);
    pool_ = CreateThreadpool(nullptr);
    if (!pool_)
    {
        throw std::runtime_error("Failed to create thread pool.");
    }
    SetThreadpoolThreadMaximum(pool_,
        static_cast<DWORD>(max_num_threads)); // Set maximum number of threads

    SetThreadpoolThreadMinimum(pool_,
        static_cast<DWORD>(min_num_threads)); // Set minimum number of threads

    cleanup_group_ = CreateThreadpoolCleanupGroup();
    if (!cleanup_group_)
    {
        CloseThreadpool(pool_);
        throw std::runtime_error("Failed to create cleanup group.");
    }
    SetThreadpoolCallbackPool(&callback_environment_, pool_);
    SetThreadpoolCallbackCleanupGroup(&callback_environment_, cleanup_group_, nullptr);
}

ThreadPool::~ThreadPool()
{
    CloseThreadpoolCleanupGroupMembers(cleanup_group_, FALSE, nullptr);
    CloseThreadpoolCleanupGroup(cleanup_group_);
    CloseThreadpool(pool_);
    DestroyThreadpoolEnvironment(&callback_environment_);
}

void ThreadPool::add_task(std::function<void()> task)
{
    auto item = std::make_unique<task_item>(task_item{ std::move(task), this });

    PTP_WORK work = CreateThreadpoolWork(work_callback, item.get(),
        &callback_environment_);
    if (!work)
    {
        // item is still owned here, so the failure does not leak the callable.
        throw std::runtime_error("Failed to create thread pool work object.");
    }
    work_items_.push_back(work);

    // The callback owns the item from the moment it is submitted.
    item.release();
    SubmitThreadpoolWork(work);
}

void ThreadPool::wait_for_tasks_to_complete()
{
    for (PTP_WORK work : work_items_)
    {
        WaitForThreadpoolWorkCallbacks(work, FALSE);
        CloseThreadpoolWork(work);
    }
    work_items_.clear();

    std::exception_ptr failure;
    {
        std::lock_guard<std::mutex> lock(exception_mutex_);
        failure = first_exception_;
        first_exception_ = nullptr;
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
    std::lock_guard<std::mutex> lock(exception_mutex_);
    if (!first_exception_)
    {
        first_exception_ = exception;
    }
}

int ThreadPool::min_num_threads() const
{
    return this->min_num_threads_;
}

int ThreadPool::max_num_threads() const
{
    return this->max_num_threads_;
}