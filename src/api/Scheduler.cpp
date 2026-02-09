/**
 * @file Scheduler.cpp
 * @brief Implementation of the task graph scheduler
 */

#include <vde/api/Scheduler.h>
#include <vde/api/ThreadPool.h>

#include <algorithm>
#include <queue>
#include <stdexcept>
#include <unordered_set>

namespace vde {

// ============================================================================
// ThreadPoolImpl — thin wrapper so Scheduler.h doesn't expose ThreadPool.h
// ============================================================================

class Scheduler::ThreadPoolImpl {
  public:
    explicit ThreadPoolImpl(size_t threadCount) : m_pool(threadCount) {}

    std::future<void> submit(std::function<void()> func) { return m_pool.submit(std::move(func)); }

    void waitAll() { m_pool.waitAll(); }

    size_t getThreadCount() const { return m_pool.getThreadCount(); }

  private:
    ThreadPool m_pool;
};

// ============================================================================
// Scheduler
// ============================================================================

Scheduler::Scheduler() = default;
Scheduler::~Scheduler() = default;

TaskId Scheduler::addTask(const TaskDescriptor& descriptor) {
    // Validate dependencies exist
    for (TaskId dep : descriptor.dependsOn) {
        if (m_tasks.find(dep) == m_tasks.end()) {
            throw std::invalid_argument("Scheduler::addTask: dependency task ID " +
                                        std::to_string(dep) + " does not exist");
        }
    }

    TaskId id = m_nextId++;
    m_tasks[id] = TaskEntry{id, descriptor};
    return id;
}

void Scheduler::removeTask(TaskId id) {
    auto it = m_tasks.find(id);
    if (it == m_tasks.end()) {
        return;
    }

    // Remove this task from all other tasks' dependency lists
    for (auto& [otherId, entry] : m_tasks) {
        auto& deps = entry.descriptor.dependsOn;
        deps.erase(std::remove(deps.begin(), deps.end(), id), deps.end());
    }

    m_tasks.erase(it);
}

void Scheduler::clear() {
    m_tasks.clear();
    m_lastExecutionOrder.clear();
}

void Scheduler::execute() {
    if (m_tasks.empty()) {
        m_lastExecutionOrder.clear();
        return;
    }

    auto sorted = topologicalSort();
    m_lastExecutionOrder = sorted;

    if (m_workerThreadCount == 0 || !m_threadPool) {
        // ---------------------------------------------------------------
        // Single-threaded path (unchanged from Phase 1)
        // ---------------------------------------------------------------
        for (TaskId id : sorted) {
            auto it = m_tasks.find(id);
            if (it != m_tasks.end() && it->second.descriptor.work) {
                it->second.descriptor.work();
            }
        }
    } else {
        // ---------------------------------------------------------------
        // Multi-threaded path — respect dependencies
        //
        // Strategy: process tasks in topological order.  Build a
        // "remaining-in-degree" map.  Walk the sorted list.  When a
        // task becomes ready (in-degree == 0):
        //   - If mainThreadOnly: execute immediately on this thread.
        //   - Else: submit to the thread pool.
        // After submitting a batch of parallel tasks, wait for all
        // of them to complete, then decrement dependents' in-degrees.
        //
        // Because the topological sort already gives a valid linear
        // order, we can simply walk it and dispatch tasks that are
        // *not* mainThreadOnly to the pool, waiting for the pool
        // after each "level" (group of tasks whose dependencies are
        // all satisfied).
        // ---------------------------------------------------------------

        // Build in-degree map and reverse adjacency (task -> its dependents)
        std::unordered_map<TaskId, int> inDegree;
        std::unordered_map<TaskId, std::vector<TaskId>> dependents;

        for (const auto& [id, entry] : m_tasks) {
            if (inDegree.find(id) == inDegree.end()) {
                inDegree[id] = 0;
            }
            for (TaskId dep : entry.descriptor.dependsOn) {
                dependents[dep].push_back(id);
                inDegree[id]++;
            }
        }

        // Priority queue: same phase-based ordering as topologicalSort()
        auto cmp = [this](TaskId a, TaskId b) {
            auto itA = m_tasks.find(a);
            auto itB = m_tasks.find(b);
            uint8_t phaseA = static_cast<uint8_t>(itA->second.descriptor.phase);
            uint8_t phaseB = static_cast<uint8_t>(itB->second.descriptor.phase);
            if (phaseA != phaseB) {
                return phaseA > phaseB;
            }
            return a > b;
        };

        std::priority_queue<TaskId, std::vector<TaskId>, decltype(cmp)> ready(cmp);

        for (const auto& [id, degree] : inDegree) {
            if (degree == 0) {
                ready.push(id);
            }
        }

        std::unordered_set<TaskId> completed;

        while (!ready.empty()) {
            // Collect all ready tasks into two batches: main-thread and pool
            std::vector<TaskId> mainTasks;
            std::vector<TaskId> poolTasks;

            // Drain all tasks at the current "frontier" that share no
            // dependencies on each other (they're all ready).
            while (!ready.empty()) {
                TaskId id = ready.top();
                ready.pop();

                auto it = m_tasks.find(id);
                if (!it->second.descriptor.work) {
                    completed.insert(id);
                    // Release dependents
                    auto depIt = dependents.find(id);
                    if (depIt != dependents.end()) {
                        for (TaskId d : depIt->second) {
                            if (--inDegree[d] == 0) {
                                ready.push(d);
                            }
                        }
                    }
                    continue;
                }

                if (it->second.descriptor.mainThreadOnly) {
                    mainTasks.push_back(id);
                } else {
                    poolTasks.push_back(id);
                }
            }

            // Submit pool tasks
            std::vector<std::future<void>> futures;
            futures.reserve(poolTasks.size());
            for (TaskId id : poolTasks) {
                auto it = m_tasks.find(id);
                futures.push_back(m_threadPool->submit(it->second.descriptor.work));
            }

            // Execute main-thread tasks sequentially
            for (TaskId id : mainTasks) {
                auto it = m_tasks.find(id);
                it->second.descriptor.work();
                completed.insert(id);
            }

            // Wait for pool tasks to finish
            for (auto& f : futures) {
                if (f.valid()) {
                    f.get();
                }
            }
            for (TaskId id : poolTasks) {
                completed.insert(id);
            }

            // Release dependents for all tasks just completed
            for (TaskId id : mainTasks) {
                auto depIt = dependents.find(id);
                if (depIt != dependents.end()) {
                    for (TaskId d : depIt->second) {
                        if (--inDegree[d] == 0) {
                            ready.push(d);
                        }
                    }
                }
            }
            for (TaskId id : poolTasks) {
                auto depIt = dependents.find(id);
                if (depIt != dependents.end()) {
                    for (TaskId d : depIt->second) {
                        if (--inDegree[d] == 0) {
                            ready.push(d);
                        }
                    }
                }
            }
        }
    }
}

size_t Scheduler::getTaskCount() const {
    return m_tasks.size();
}

bool Scheduler::hasTask(TaskId id) const {
    return m_tasks.find(id) != m_tasks.end();
}

std::string Scheduler::getTaskName(TaskId id) const {
    auto it = m_tasks.find(id);
    if (it != m_tasks.end()) {
        return it->second.descriptor.name;
    }
    return "";
}

const std::vector<TaskId>& Scheduler::getLastExecutionOrder() const {
    return m_lastExecutionOrder;
}

void Scheduler::setWorkerThreadCount(size_t count) {
    m_workerThreadCount = count;
    if (count > 0) {
        m_threadPool = std::make_unique<ThreadPoolImpl>(count);
    } else {
        m_threadPool.reset();
    }
}

size_t Scheduler::getWorkerThreadCount() const {
    return m_workerThreadCount;
}

std::vector<TaskId> Scheduler::topologicalSort() const {
    // Kahn's algorithm with phase-based tiebreaking via priority queue
    // Build in-degree map and adjacency list
    std::unordered_map<TaskId, int> inDegree;
    std::unordered_map<TaskId, std::vector<TaskId>> dependents;  // task -> tasks that depend on it

    for (const auto& [id, entry] : m_tasks) {
        if (inDegree.find(id) == inDegree.end()) {
            inDegree[id] = 0;
        }
        for (TaskId dep : entry.descriptor.dependsOn) {
            // dep -> id (id depends on dep)
            dependents[dep].push_back(id);
            inDegree[id]++;
        }
    }

    // Priority queue: smaller phase first, then smaller task ID as tiebreaker
    // Using a min-heap with (phase, taskId)
    auto cmp = [this](TaskId a, TaskId b) {
        auto itA = m_tasks.find(a);
        auto itB = m_tasks.find(b);
        uint8_t phaseA = static_cast<uint8_t>(itA->second.descriptor.phase);
        uint8_t phaseB = static_cast<uint8_t>(itB->second.descriptor.phase);
        if (phaseA != phaseB) {
            return phaseA > phaseB;  // min-heap: smaller phase = higher priority
        }
        return a > b;  // min-heap: smaller ID = higher priority (stable registration order)
    };

    std::priority_queue<TaskId, std::vector<TaskId>, decltype(cmp)> ready(cmp);

    // Seed with zero in-degree tasks
    for (const auto& [id, degree] : inDegree) {
        if (degree == 0) {
            ready.push(id);
        }
    }

    std::vector<TaskId> result;
    result.reserve(m_tasks.size());

    while (!ready.empty()) {
        TaskId current = ready.top();
        ready.pop();
        result.push_back(current);

        auto depIt = dependents.find(current);
        if (depIt != dependents.end()) {
            for (TaskId dependent : depIt->second) {
                inDegree[dependent]--;
                if (inDegree[dependent] == 0) {
                    ready.push(dependent);
                }
            }
        }
    }

    if (result.size() != m_tasks.size()) {
        throw std::runtime_error("Scheduler::execute: cycle detected in task graph");
    }

    return result;
}

}  // namespace vde
