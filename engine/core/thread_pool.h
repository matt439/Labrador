#pragma once

#include <windows.h>
#include <exception>
#include <functional>
#include <mutex>
#include <vector>

namespace labrador
{
    class ThreadPool
    {
    public:
        ThreadPool(int min_num_threads, int max_num_threads);
        ~ThreadPool();

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        void add_task(std::function<void()> task);

        // Blocks until every submitted task has finished. If any task threw, the
        // first such exception is rethrown here, on the calling thread. An
        // exception escaping a Win32 thread-pool callback would otherwise
        // terminate the process with no diagnostic.
        void wait_for_tasks_to_complete();

        int min_num_threads() const;
        int max_num_threads() const;

    private:
        // Owns the callable for the lifetime of one submission, plus the back
        // pointer the callback needs to report a failure.
        struct task_item
        {
            std::function<void()> task;
            ThreadPool* pool = nullptr;
        };

        static void CALLBACK work_callback(PTP_CALLBACK_INSTANCE,
            PVOID parameter, PTP_WORK);

        void record_exception(std::exception_ptr exception);

        int min_num_threads_ = -1;
        int max_num_threads_ = -1;
        std::vector<PTP_WORK> work_items_;
        TP_CALLBACK_ENVIRON callback_environment_ = {};
        PTP_POOL pool_ = nullptr;
        PTP_CLEANUP_GROUP cleanup_group_ = nullptr;

        std::mutex exception_mutex_;
        std::exception_ptr first_exception_;
    };
}
