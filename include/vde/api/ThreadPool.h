#pragma once

/**
 * @file ThreadPool.h
 * @brief Thread pool for parallel task execution in VDE
 *
 * Provides a simple thread pool that accepts callable tasks and
 * distributes them across a fixed number of worker threads.
 * Used by the Scheduler to parallelize independent tasks (e.g.,
 * per-scene physics simulation).
 */

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace vde {

/**
 * @brief A simple fixed-size thread pool for parallel task execution.
 *
 * Workers pull tasks from a shared queue and execute them. The pool
 * supports graceful shutdown — the destructor joins all workers after
 * draining pending work.
 *
 * If constructed with threadCount == 0, submitted tasks are executed
 * inline on the calling thread (single-threaded fallback).
 *
 * @example
 * @code
 * vde::ThreadPool pool(4);
 * auto f1 = pool.submit([]{ doWork1(); });
 * auto f2 = pool.submit([]{ doWork2(); });
 * pool.waitAll();
 * @endcode
 */
class ThreadPool {
  public:
    /**
     * @brief Construct a thread pool with the given number of worker threads.
     * @param threadCount Number of worker threads to spawn.  0 = inline execution.
     */
    explicit ThreadPool(size_t threadCount = 0);

    /**
     * @brief Destructor — signals shutdown and joins all workers.
     *
     * Any tasks still in the queue will be executed before shutdown completes.
     */
    ~ThreadPool();

    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /**
     * @brief Submit a task for execution.
     *
     * If threadCount == 0, the task is executed immediately on the
     * calling thread.  Otherwise it is enqueued for a worker.
     *
     * @param func Callable to execute
     * @return std::future<void> that completes when the task finishes
     */
    std::future<void> submit(std::function<void()> func);

    /**
     * @brief Block until all previously submitted tasks are complete.
     *
     * This does NOT prevent new tasks from being submitted while waiting.
     * It simply waits for every future returned by prior submit() calls
     * to be ready.
     */
    void waitAll();

    /**
     * @brief Get the number of worker threads.
     * @return 0 if running in inline mode, otherwise the thread count.
     */
    size_t getThreadCount() const;

    /**
     * @brief Get the thread IDs of all worker threads.
     *
     * Useful for verifying that tasks execute on different threads.
     * Returns empty vector if threadCount == 0.
     */
    std::vector<std::thread::id> getWorkerThreadIds() const;

  private:
    void workerLoop();

    size_t m_threadCount;
    std::vector<std::thread> m_workers;
    std::queue<std::packaged_task<void()>> m_taskQueue;

    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::condition_variable m_doneCondition;  ///< Signalled when a task finishes
    bool m_shutdown = false;
    size_t m_busyCount = 0;  ///< Number of workers currently executing a task
};

}  // namespace vde
