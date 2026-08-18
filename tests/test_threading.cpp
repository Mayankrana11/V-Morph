#include <gtest/gtest.h>
#include "threading/RealtimeThread.h"
#include "threading/ThreadUtils.h"

using namespace rtvcc;

TEST(ThreadingTest, RealtimeThreadBasic) {
    std::atomic<int> counter{0};
    RealtimeThread thread([&counter](std::atomic<bool>& stop) {
        while (!stop.load()) {
            counter.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    thread.stop();
    thread.join();

    EXPECT_GT(counter.load(), 0);
}

TEST(ThreadingTest, RealtimeThreadMoveConstructor) {
    std::atomic<int> counter{0};
    RealtimeThread thread1([&counter](std::atomic<bool>& stop) {
        while (!stop.load()) {
            counter.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    RealtimeThread thread2 = std::move(thread1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    thread2.stop();
    thread2.join();

    EXPECT_GT(counter.load(), 0);
}

TEST(ThreadingTest, SpinLockBasic) {
    SpinLock lock;
    int shared = 0;

    auto worker = [&lock, &shared]() {
        for (int i = 0; i < 10000; ++i) {
            lock.lock();
            ++shared;
            lock.unlock();
        }
    };

    std::thread t1(worker);
    std::thread t2(worker);
    t1.join();
    t2.join();

    EXPECT_EQ(shared, 20000);
}

TEST(ThreadingTest, SpinLockTryLock) {
    SpinLock lock;
    EXPECT_TRUE(lock.tryLock());
    EXPECT_FALSE(lock.tryLock());
    lock.unlock();
    EXPECT_TRUE(lock.tryLock());
    lock.unlock();
}

TEST(ThreadingTest, ThreadUtilsSetName) {
    // Just verify it doesn't crash
    ThreadUtils::setCurrentThreadName("TestThread");
    EXPECT_EQ(ThreadUtils::getCurrentThreadId(), std::this_thread::get_id());
}

TEST(ThreadingTest, ThreadUtilsHardwareConcurrency) {
    size_t concurrency = ThreadUtils::getHardwareConcurrency();
    EXPECT_GT(concurrency, 0);
}

TEST(ThreadingTest, ThreadUtilsSleep) {
    auto start = std::chrono::steady_clock::now();
    ThreadUtils::sleepFor(std::chrono::milliseconds(10));
    auto end = std::chrono::steady_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_GE(elapsed, 8);  // Allow some slack
    EXPECT_LE(elapsed, 50);
}