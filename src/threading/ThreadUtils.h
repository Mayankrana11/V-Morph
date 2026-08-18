#pragma once

#include "RealtimeThread.h"
#include <vector>

namespace rtvcc {

// SPSC lock-free queue for generic types
// Uses Cameron314's concurrentqueue or similar
template <typename T>
class SpscQueue {
public:
    explicit SpscQueue(size_t capacity);
    ~SpscQueue();

    SpscQueue(const SpscQueue&) = delete;
    SpscQueue& operator=(const SpscQueue&) = delete;
    SpscQueue(SpscQueue&&) = default;
    SpscQueue& operator=(SpscQueue&&) = default;

    // Producer side
    bool tryPush(const T& item);
    bool tryPush(T&& item);
    size_t pushBulk(const T* items, size_t count);
    size_t tryPushBulk(const T* items, size_t count);

    // Consumer side
    bool tryPop(T& item);
    size_t popBulk(T* items, size_t count);
    size_t tryPopBulk(T* items, size_t count);

    size_t size() const;
    size_t capacity() const;
    bool empty() const;
    bool full() const;

    void clear();

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

// Thread pool for non-real-time work
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = 0);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename Func, typename... Args>
    auto enqueue(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>;

    size_t getThreadCount() const;
    size_t getQueueSize() const;

    void waitForAll();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

// Barrier for synchronizing threads
class Barrier {
public:
    explicit Barrier(size_t count);
    void wait();
    void arriveAndWait();
    void arriveAndDrop();

private:
    std::atomic<size_t> count_;
    std::atomic<size_t> spaces_;
    std::atomic<size_t> generation_;
};

// Spinlock for very short critical sections (non-real-time only)
class SpinLock {
public:
    void lock();
    bool tryLock();
    void unlock();

private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

} // namespace rtvcc