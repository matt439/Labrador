#pragma once

#include <functional>
#include <memory>

namespace labrador
{
    // A pool of worker threads, and the one place a task's exception is caught
    // and carried back to whoever waited for it.
    //
    // NO <windows.h> HERE, WHICH IS WHAT Impl IS FOR. This header used to open
    // with it, because the private members below were PTP_WORK, PTP_POOL,
    // PTP_CLEANUP_GROUP and a TP_CALLBACK_ENVIRON, and a static callback
    // spelt in PVOID. That handed the whole Win32 surface to every module that
    // wanted a worker - engine/scene/scene.cpp and engine/app/application.h
    // are the two - out of core/, which is the one module everything else is
    // allowed to lean on. core/registry.h had already refused exactly this,
    // deliberately, and says so: its COM-facing specialisation lives where COM
    // is already in scope "rather than dragging <wrl/client.h> in here". The
    // standard was stated in this folder and this file was not held to it.
    //
    // WHAT IS BEHIND THE POINTER IS UNCHANGED, and that is deliberate too. The
    // implementation is still the Win32 thread pool - CreateThreadpoolWork,
    // SubmitThreadpoolWork, a cleanup group - because it works, it is tested,
    // and replacing it with std::thread would be rewriting a working primitive
    // for a platform this engine does not build for yet (T1). What moved is
    // where the API is named, not which API it is.
    //
    // THE INDIRECTION IS NOT ON A HOT PATH. Scene::draw submits one task per
    // slice of the view list per frame, and the slices are bounded by
    // max_num_threads - a handful - so one pointer hop per submission is
    // nothing T8 has an opinion about, where a virtual call per sprite would
    // be.
    class ThreadPool
    {
    public:
        ThreadPool(int min_num_threads, int max_num_threads);
        ~ThreadPool();

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        // ONE THREAD SUBMITS. This is not a work-stealing queue a worker can
        // add to: the submitted-work list is unguarded, so add_task and
        // wait_for_tasks_to_complete belong to whichever thread owns the pool.
        // Scene::draw is that thread, and it waits before returning. What is
        // guarded is the other direction - a task's exception, recorded from
        // whichever worker ran it.
        void add_task(std::function<void()> task);

        // Blocks until every submitted task has finished. If any task threw, the
        // first such exception is rethrown here, on the calling thread. An
        // exception escaping a Win32 thread-pool callback would otherwise
        // terminate the process with no diagnostic.
        void wait_for_tasks_to_complete();

        int min_num_threads() const;
        int max_num_threads() const;

    private:
        // Declared here and defined in the .cpp, so the platform types stay in
        // the translation unit that calls them. Not moveable: the callback a
        // submitted task holds points at one of these, so the address has to
        // stay put for the pool's whole life - which a unique_ptr gives for
        // free and a member would not.
        class Impl;
        std::unique_ptr<Impl> impl_;
    };
}
