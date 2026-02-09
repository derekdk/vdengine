/**
 * @file ThreadPool_test.cpp
 * @brief Unit tests for the ThreadPool class
 */

#include <vde/api/Scheduler.h>
#include <vde/api/ThreadPool.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace vde::test {

// ============================================================================
// ThreadPool Tests
// ============================================================================

class ThreadPoolTest : public ::testing::Test {};

// ---------- Construction & basic properties ----------

TEST_F(ThreadPoolTest, ZeroThreadCountCreatesInlinePool) {
    ThreadPool pool(0);
    EXPECT_EQ(pool.getThreadCount(), 0u);
    EXPECT_TRUE(pool.getWorkerThreadIds().empty());
}

TEST_F(ThreadPoolTest, NonZeroThreadCountCreatesWorkers) {
    ThreadPool pool(4);
    EXPECT_EQ(pool.getThreadCount(), 4u);
    EXPECT_EQ(pool.getWorkerThreadIds().size(), 4u);
}

TEST_F(ThreadPoolTest, WorkerThreadIdsAreUnique) {
    ThreadPool pool(4);
    auto ids = pool.getWorkerThreadIds();
    std::set<std::thread::id> unique(ids.begin(), ids.end());
    EXPECT_EQ(unique.size(), 4u);
}

// ---------- Single task ----------

TEST_F(ThreadPoolTest, SubmitSingleTaskInlineCompletes) {
    ThreadPool pool(0);
    bool executed = false;
    auto future = pool.submit([&]() { executed = true; });
    future.get();
    EXPECT_TRUE(executed);
}

TEST_F(ThreadPoolTest, SubmitSingleTaskThreadedCompletes) {
    ThreadPool pool(2);
    std::atomic<bool> executed{false};
    auto future = pool.submit([&]() { executed.store(true); });
    future.get();
    EXPECT_TRUE(executed.load());
}

// ---------- Multiple independent tasks ----------

TEST_F(ThreadPoolTest, SubmitMultipleTasksAllComplete) {
    ThreadPool pool(4);
    constexpr int N = 100;
    std::atomic<int> counter{0};

    std::vector<std::future<void>> futures;
    futures.reserve(N);
    for (int i = 0; i < N; ++i) {
        futures.push_back(pool.submit([&]() { counter.fetch_add(1); }));
    }

    for (auto& f : futures) {
        f.get();
    }
    EXPECT_EQ(counter.load(), N);
}

// ---------- waitAll ----------

TEST_F(ThreadPoolTest, WaitAllBlocksUntilDone) {
    ThreadPool pool(4);
    constexpr int N = 50;
    std::atomic<int> counter{0};

    for (int i = 0; i < N; ++i) {
        pool.submit([&]() {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            counter.fetch_add(1);
        });
    }

    pool.waitAll();
    EXPECT_EQ(counter.load(), N);
}

TEST_F(ThreadPoolTest, WaitAllWithZeroThreadsIsNoOp) {
    ThreadPool pool(0);
    bool executed = false;
    pool.submit([&]() { executed = true; });
    pool.waitAll();  // should not hang
    EXPECT_TRUE(executed);
}

// ---------- Destructor joins cleanly ----------

TEST_F(ThreadPoolTest, DestructorJoinsCleanlyWithPendingTasks) {
    std::atomic<int> counter{0};
    {
        ThreadPool pool(2);
        for (int i = 0; i < 20; ++i) {
            pool.submit([&]() {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
                counter.fetch_add(1);
            });
        }
        // Destructor runs here — should join all workers
    }
    // All tasks should have completed before destructor returned
    EXPECT_EQ(counter.load(), 20);
}

// ---------- Tasks execute on different threads ----------

TEST_F(ThreadPoolTest, TasksExecuteOnWorkerThreads) {
    ThreadPool pool(4);
    std::mutex mtx;
    std::set<std::thread::id> taskThreadIds;

    constexpr int N = 20;
    std::vector<std::future<void>> futures;
    for (int i = 0; i < N; ++i) {
        futures.push_back(pool.submit([&]() {
            // Sleep briefly so tasks overlap across workers
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            std::lock_guard<std::mutex> lock(mtx);
            taskThreadIds.insert(std::this_thread::get_id());
        }));
    }

    for (auto& f : futures) {
        f.get();
    }

    // At least 2 different threads should have been used
    EXPECT_GE(taskThreadIds.size(), 2u);

    // Worker threads should NOT include the main thread
    auto mainThreadId = std::this_thread::get_id();
    EXPECT_EQ(taskThreadIds.count(mainThreadId), 0u);
}

TEST_F(ThreadPoolTest, InlineTasksRunOnCallingThread) {
    ThreadPool pool(0);
    std::thread::id taskThread;
    auto future = pool.submit([&]() { taskThread = std::this_thread::get_id(); });
    future.get();
    EXPECT_EQ(taskThread, std::this_thread::get_id());
}

// ---------- Rapid submit/wait cycles ----------

TEST_F(ThreadPoolTest, RepeatedSubmitWaitCycles) {
    ThreadPool pool(2);
    for (int cycle = 0; cycle < 10; ++cycle) {
        std::atomic<int> counter{0};
        for (int i = 0; i < 10; ++i) {
            pool.submit([&]() { counter.fetch_add(1); });
        }
        pool.waitAll();
        EXPECT_EQ(counter.load(), 10);
    }
}

// ============================================================================
// Scheduler + ThreadPool Integration Tests
// ============================================================================

class SchedulerThreadPoolTest : public ::testing::Test {
  protected:
    Scheduler scheduler;
    std::vector<std::string> executionLog;
    std::mutex logMutex;

    TaskDescriptor makeLoggingTask(const std::string& name, TaskPhase phase,
                                   const std::vector<TaskId>& deps = {}, bool mainThread = true) {
        return {name, phase,
                [this, name]() {
                    std::lock_guard<std::mutex> lock(logMutex);
                    executionLog.push_back(name);
                },
                deps, mainThread};
    }
};

TEST_F(SchedulerThreadPoolTest, DefaultIsZeroWorkers) {
    EXPECT_EQ(scheduler.getWorkerThreadCount(), 0u);
}

TEST_F(SchedulerThreadPoolTest, SetWorkerThreadCountUpdatesCount) {
    scheduler.setWorkerThreadCount(4);
    EXPECT_EQ(scheduler.getWorkerThreadCount(), 4u);
}

TEST_F(SchedulerThreadPoolTest, SetWorkerThreadCountToZeroResetsToSingleThreaded) {
    scheduler.setWorkerThreadCount(4);
    scheduler.setWorkerThreadCount(0);
    EXPECT_EQ(scheduler.getWorkerThreadCount(), 0u);
}

TEST_F(SchedulerThreadPoolTest, SingleThreadedExecutionOrderPreserved) {
    scheduler.setWorkerThreadCount(0);

    auto t1 = scheduler.addTask(makeLoggingTask("input", TaskPhase::Input));
    auto t2 = scheduler.addTask(makeLoggingTask("update", TaskPhase::GameLogic, {t1}));
    scheduler.addTask(makeLoggingTask("render", TaskPhase::Render, {t2}));

    scheduler.execute();

    ASSERT_EQ(executionLog.size(), 3u);
    EXPECT_EQ(executionLog[0], "input");
    EXPECT_EQ(executionLog[1], "update");
    EXPECT_EQ(executionLog[2], "render");
}

TEST_F(SchedulerThreadPoolTest, MultiThreadedExecutionRespectsDepOrder) {
    scheduler.setWorkerThreadCount(2);

    // Chain: A -> B -> C (all main-thread for predictable ordering)
    auto a = scheduler.addTask(makeLoggingTask("A", TaskPhase::Input));
    auto b = scheduler.addTask(makeLoggingTask("B", TaskPhase::GameLogic, {a}));
    scheduler.addTask(makeLoggingTask("C", TaskPhase::Render, {b}));

    scheduler.execute();

    ASSERT_EQ(executionLog.size(), 3u);
    EXPECT_EQ(executionLog[0], "A");
    EXPECT_EQ(executionLog[1], "B");
    EXPECT_EQ(executionLog[2], "C");
}

TEST_F(SchedulerThreadPoolTest, IndependentNonMainThreadTasksRunInParallel) {
    scheduler.setWorkerThreadCount(4);

    std::mutex mtx;
    std::set<std::thread::id> threadIds;

    auto work = [&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::lock_guard<std::mutex> lock(mtx);
        threadIds.insert(std::this_thread::get_id());
    };

    auto root = scheduler.addTask({"root", TaskPhase::Input, []() {}, {}, true});

    // Add 4 independent non-main-thread tasks
    for (int i = 0; i < 4; ++i) {
        scheduler.addTask({"worker" + std::to_string(i), TaskPhase::Physics, work, {root}, false});
    }

    scheduler.execute();

    // At least 2 different threads should have been used (could be fewer under load)
    EXPECT_GE(threadIds.size(), 2u);
}

TEST_F(SchedulerThreadPoolTest, MainThreadOnlyTasksRunOnMainThread) {
    scheduler.setWorkerThreadCount(2);

    std::thread::id capturedId;
    auto mainId = std::this_thread::get_id();

    scheduler.addTask({"main_task",
                       TaskPhase::Input,
                       [&]() { capturedId = std::this_thread::get_id(); },
                       {},
                       true});

    scheduler.execute();

    EXPECT_EQ(capturedId, mainId);
}

TEST_F(SchedulerThreadPoolTest, EmptyGraphWithWorkersIsNoOp) {
    scheduler.setWorkerThreadCount(2);
    scheduler.execute();  // should not hang or crash
    EXPECT_TRUE(scheduler.getLastExecutionOrder().empty());
}

TEST_F(SchedulerThreadPoolTest, MixedMainAndPoolTasksRespectDeps) {
    scheduler.setWorkerThreadCount(2);

    // Graph:  input(main) -> physics1(pool), physics2(pool) -> render(main)
    std::atomic<int> physicsCounter{0};

    auto input = scheduler.addTask({"input",
                                    TaskPhase::Input,
                                    [this]() {
                                        std::lock_guard<std::mutex> lock(logMutex);
                                        executionLog.push_back("input");
                                    },
                                    {},
                                    true});

    auto p1 = scheduler.addTask(
        {"physics1", TaskPhase::Physics, [&]() { physicsCounter.fetch_add(1); }, {input}, false});

    auto p2 = scheduler.addTask(
        {"physics2", TaskPhase::Physics, [&]() { physicsCounter.fetch_add(1); }, {input}, false});

    scheduler.addTask({"render",
                       TaskPhase::Render,
                       [this, &physicsCounter]() {
                           std::lock_guard<std::mutex> lock(logMutex);
                           // By the time render runs, both physics tasks must be done
                           executionLog.push_back("render_" +
                                                  std::to_string(physicsCounter.load()));
                       },
                       {p1, p2},
                       true});

    scheduler.execute();

    // Verify render ran after both physics tasks
    ASSERT_GE(executionLog.size(), 2u);
    EXPECT_EQ(executionLog[0], "input");
    EXPECT_EQ(executionLog.back(), "render_2");
}

}  // namespace vde::test
