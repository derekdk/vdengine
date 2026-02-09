/**
 * @file ThreadPool.cpp
 * @brief Implementation of the ThreadPool class
 */

#include <vde/api/ThreadPool.h>

namespace vde {

ThreadPool::ThreadPool(size_t threadCount) : m_threadCount(threadCount) {
    m_workers.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i) {
        m_workers.emplace_back(&ThreadPool::workerLoop, this);
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_shutdown = true;
    }
    m_condition.notify_all();

    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

std::future<void> ThreadPool::submit(std::function<void()> func) {
    if (m_threadCount == 0) {
        // Inline execution — run immediately on calling thread
        std::packaged_task<void()> task(std::move(func));
        auto future = task.get_future();
        task();
        return future;
    }

    std::packaged_task<void()> task(std::move(func));
    auto future = task.get_future();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_taskQueue.push(std::move(task));
    }
    m_condition.notify_one();

    return future;
}

void ThreadPool::waitAll() {
    if (m_threadCount == 0) {
        // In inline mode tasks are already complete when submit() returns.
        return;
    }

    // Wait until the queue is empty AND no worker is busy
    std::unique_lock<std::mutex> lock(m_mutex);
    m_doneCondition.wait(lock, [this]() { return m_taskQueue.empty() && m_busyCount == 0; });
}

size_t ThreadPool::getThreadCount() const {
    return m_threadCount;
}

std::vector<std::thread::id> ThreadPool::getWorkerThreadIds() const {
    std::vector<std::thread::id> ids;
    ids.reserve(m_workers.size());
    for (const auto& w : m_workers) {
        ids.push_back(w.get_id());
    }
    return ids;
}

void ThreadPool::workerLoop() {
    while (true) {
        std::packaged_task<void()> task;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_condition.wait(lock, [this]() { return m_shutdown || !m_taskQueue.empty(); });

            if (m_shutdown && m_taskQueue.empty()) {
                return;
            }

            task = std::move(m_taskQueue.front());
            m_taskQueue.pop();
            ++m_busyCount;
        }

        task();

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            --m_busyCount;
        }
        m_doneCondition.notify_all();
    }
}

}  // namespace vde
