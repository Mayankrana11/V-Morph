#include "ThreadUtils.h"
#include <atomic>
#include <vector>
#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace rtvcc {

// SpscQueue implementation using atomic indices
template <typename T>
struct SpscQueue<T>::Impl {
    std::vector<T> buffer_;
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
    size_t capacity_;

    Impl(size_t capacity) : buffer_(capacity), capacity_(capacity) {}
};

template <typename T>
SpscQueue<T>::SpscQueue(size_t capacity) : pimpl_(std::make_unique<Impl>(capacity)) {}

template <typename T>
SpscQueue<T>::~SpscQueue() = default;

template <typename T>
bool SpscQueue<T>::tryPush(const T& item) {
    size_t head = pimpl_->head_.load(std::memory_order_relaxed);
    size_t next_head = (head + 1) % pimpl_->capacity_;
    if (next_head == pimpl_->tail_.load(std::memory_order_acquire)) {
        return false;
    }
    pimpl_->buffer_[head] = item;
    pimpl_->head_.store(next_head, std::memory_order_release);
    return true;
}

template <typename T>
bool SpscQueue<T>::tryPush(T&& item) {
    size_t head = pimpl_->head_.load(std::memory_order_relaxed);
    size_t next_head = (head + 1) % pimpl_->capacity_;
    if (next_head == pimpl_->tail_.load(std::memory_order_acquire)) {
        return false;
    }
    pimpl_->buffer_[head] = std::move(item);
    pimpl_->head_.store(next_head, std::memory_order_release);
    return true;
}

template <typename T>
size_t SpscQueue<T>::pushBulk(const T* items, size_t count) {
    size_t pushed = 0;
    while (pushed < count && tryPush(items[pushed])) {
        ++pushed;
    }
    return pushed;
}

template <typename T>
size_t SpscQueue<T>::tryPushBulk(const T* items, size_t count) {
    return pushBulk(items, count);
}

template <typename T>
bool SpscQueue<T>::tryPop(T& item) {
    size_t tail = pimpl_->tail_.load(std::memory_order_relaxed);
    if (tail == pimpl_->head_.load(std::memory_order_acquire)) {
        return false;
    }
    item = std::move(pimpl_->buffer_[tail]);
    pimpl_->tail_.store((tail + 1) % pimpl_->capacity_, std::memory_order_release);
    return true;
}

template <typename T>
size_t SpscQueue<T>::popBulk(T* items, size_t count) {
    size_t popped = 0;
    while (popped < count && tryPop(items[popped])) {
        ++popped;
    }
    return popped;
}

template <typename T>
size_t SpscQueue<T>::tryPopBulk(T* items, size_t count) {
    return popBulk(items, count);
}

template <typename T>
size_t SpscQueue<T>::size() const {
    size_t head = pimpl_->head_.load(std::memory_order_acquire);
    size_t tail = pimpl_->tail_.load(std::memory_order_acquire);
    return (head >= tail) ? (head - tail) : (pimpl_->capacity_ - tail + head);
}

template <typename T>
size_t SpscQueue<T>::capacity() const {
    return pimpl_->capacity_ - 1;
}

template <typename T>
bool SpscQueue<T>::empty() const {
    return pimpl_->head_.load(std::memory_order_acquire) == pimpl_->tail_.load(std::memory_order_acquire);
}

template <typename T>
bool SpscQueue<T>::full() const {
    size_t head = pimpl_->head_.load(std::memory_order_acquire);
    size_t next_head = (head + 1) % pimpl_->capacity_;
    return next_head == pimpl_->tail_.load(std::memory_order_acquire);
}

template <typename T>
void SpscQueue<T>::clear() {
    pimpl_->head_.store(0, std::memory_order_release);
    pimpl_->tail_.store(0, std::memory_order_release);
}

// Explicit instantiation for common types
template class SpscQueue<float>;
template class SpscQueue<double>;
template class SpscQueue<int>;
template class SpscQueue<std::vector<float>>;

// ThreadPool implementation
class ThreadPool::Impl {
public:
    Impl(size_t num_threads) : stop_(false) {
        if (num_threads == 0) num_threads = std::thread::hardware_concurrency();
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] { workerLoop(); });
        }
    }

    ~Impl() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
    }

    template <typename Func, typename... Args>
    auto enqueue(Func&& func, Args&&... args) -> std::future<decltype(func(args...))> {
        using ReturnType = decltype(func(args...));
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
        );
        std::future<ReturnType> future = task->get_future();
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            tasks_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one();
        return future;
    }

    size_t getThreadCount() const { return workers_.size(); }
    size_t getQueueSize() const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return tasks_.size();
    }

    void waitForAll() {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        cv_.wait(lock, [this] { return tasks_.empty(); });
    }

private:
    void workerLoop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) return;
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    bool stop_;
};

ThreadPool::ThreadPool(size_t num_threads) : pimpl_(std::make_unique<Impl>(num_threads)) {}
ThreadPool::~ThreadPool() = default;

template <typename Func, typename... Args>
auto ThreadPool::enqueue(Func&& func, Args&&... args) -> std::future<decltype(func(args...))> {
    return pimpl_->enqueue(std::forward<Func>(func), std::forward<Args>(args)...);
}

size_t ThreadPool::getThreadCount() const { return pimpl_->getThreadCount(); }
size_t ThreadPool::getQueueSize() const { return pimpl_->getQueueSize(); }
void ThreadPool::waitForAll() { pimpl_->waitForAll(); }

// Barrier implementation
Barrier::Barrier(size_t count) : count_(count), spaces_(count), generation_(0) {}

void Barrier::wait() {
    size_t gen = generation_.load(std::memory_order_acquire);
    size_t spaces = spaces_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (spaces == 0) {
        spaces_.store(count_, std::memory_order_release);
        generation_.fetch_add(1, std::memory_order_release);
    } else {
        while (generation_.load(std::memory_order_acquire) == gen) {
            std::this_thread::yield();
        }
    }
}

void Barrier::arriveAndWait() {
    wait();
}

void Barrier::arriveAndDrop() {
    size_t spaces = spaces_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (spaces == 0) {
        count_.fetch_sub(1, std::memory_order_release);
        spaces_.store(count_.load(), std::memory_order_release);
        generation_.fetch_add(1, std::memory_order_release);
    }
}

// SpinLock implementation
void SpinLock::lock() {
    while (flag_.test_and_set(std::memory_order_acquire)) {
        // Spin
#if defined(_MSC_VER)
        _mm_pause();
#else
        __builtin_ia32_pause();
#endif
    }
}

bool SpinLock::tryLock() {
    return !flag_.test_and_set(std::memory_order_acquire);
}

void SpinLock::unlock() {
    flag_.clear(std::memory_order_release);
}

} // namespace rtvcc