#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace rtvcc {

// Lock-free Single-Producer Single-Consumer (SPSC) ring buffer
// Designed for real-time audio: no allocations, no locks in push/pop
template <typename T>
class SpscRingBuffer {
public:
    explicit SpscRingBuffer(size_t capacity)
        : capacity_(nextPowerOfTwo(capacity))
        , mask_(capacity_ - 1)
        , buffer_(std::make_unique<T[]>(capacity_))
        , write_pos_(0)
        , read_pos_(0)
    {}

    ~SpscRingBuffer() = default;

    // Non-copyable, movable
    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;
    SpscRingBuffer(SpscRingBuffer&&) = default;
    SpscRingBuffer& operator=(SpscRingBuffer&&) = default;

    // Push single element (producer side)
    // Returns true if pushed, false if full
    bool push(const T& item) {
        const size_t write = write_pos_.load(std::memory_order_relaxed);
        const size_t next_write = (write + 1) & mask_;
        if (next_write == read_pos_.load(std::memory_order_acquire)) {
            return false;  // full
        }
        buffer_[write] = item;
        write_pos_.store(next_write, std::memory_order_release);
        return true;
    }

    // Push multiple elements (producer side)
    // Returns number of elements pushed
    size_t push(const T* items, size_t count) {
        size_t write = write_pos_.load(std::memory_order_relaxed);
        size_t read = read_pos_.load(std::memory_order_acquire);
        size_t available = (read > write) ? (read - write - 1) : (capacity_ - write + read - 1);
        size_t to_push = (count < available) ? count : available;

        for (size_t i = 0; i < to_push; ++i) {
            buffer_[write] = items[i];
            write = (write + 1) & mask_;
        }
        write_pos_.store(write, std::memory_order_release);
        return to_push;
    }

    // Pop single element (consumer side)
    // Returns true if popped, false if empty
    bool pop(T& item) {
        const size_t read = read_pos_.load(std::memory_order_relaxed);
        if (read == write_pos_.load(std::memory_order_acquire)) {
            return false;  // empty
        }
        item = buffer_[read];
        read_pos_.store((read + 1) & mask_, std::memory_order_release);
        return true;
    }

    // Pop multiple elements (consumer side)
    // Returns number of elements popped
    size_t pop(T* items, size_t count) {
        size_t read = read_pos_.load(std::memory_order_relaxed);
        size_t write = write_pos_.load(std::memory_order_acquire);
        size_t available = (write >= read) ? (write - read) : (capacity_ - read + write);
        size_t to_pop = (count < available) ? count : available;

        for (size_t i = 0; i < to_pop; ++i) {
            items[i] = buffer_[read];
            read = (read + 1) & mask_;
        }
        read_pos_.store(read, std::memory_order_release);
        return to_pop;
    }

    // Peek at elements without removing (consumer side)
    size_t peek(T* items, size_t count) const {
        size_t read = read_pos_.load(std::memory_order_relaxed);
        size_t write = write_pos_.load(std::memory_order_acquire);
        size_t available = (write >= read) ? (write - read) : (capacity_ - read + write);
        size_t to_peek = (count < available) ? count : available;

        for (size_t i = 0; i < to_peek; ++i) {
            items[i] = buffer_[(read + i) & mask_];
        }
        return to_peek;
    }

    // Discard elements (consumer side)
    size_t discard(size_t count) {
        size_t read = read_pos_.load(std::memory_order_relaxed);
        size_t write = write_pos_.load(std::memory_order_acquire);
        size_t available = (write >= read) ? (write - read) : (capacity_ - read + write);
        size_t to_discard = (count < available) ? count : available;

        read_pos_.store((read + to_discard) & mask_, std::memory_order_release);
        return to_discard;
    }

    size_t size() const {
        size_t write = write_pos_.load(std::memory_order_acquire);
        size_t read = read_pos_.load(std::memory_order_acquire);
        return (write >= read) ? (write - read) : (capacity_ - read + write);
    }

    size_t capacity() const { return capacity_ - 1; }  // -1 for empty/full distinction
    size_t available() const { return capacity() - size(); }

    bool empty() const {
        return read_pos_.load(std::memory_order_acquire) == write_pos_.load(std::memory_order_acquire);
    }

    bool full() const {
        size_t write = write_pos_.load(std::memory_order_acquire);
        size_t next_write = (write + 1) & mask_;
        return next_write == read_pos_.load(std::memory_order_acquire);
    }

    void clear() {
        read_pos_.store(0, std::memory_order_release);
        write_pos_.store(0, std::memory_order_release);
    }

    // Get write pointer for direct writing (producer side)
    // Returns pair of (first_chunk_size, second_chunk_size)
    // Use with commitWrite() after writing
    std::pair<size_t, size_t> prepareWrite(size_t count) {
        size_t write = write_pos_.load(std::memory_order_relaxed);
        size_t read = read_pos_.load(std::memory_order_acquire);
        size_t available = (read > write) ? (read - write - 1) : (capacity_ - write + read - 1);
        count = (count < available) ? count : available;

        size_t first_chunk = std::min(count, capacity_ - write);
        size_t second_chunk = count - first_chunk;
        return {first_chunk, second_chunk};
    }

    T* getWritePtr(size_t index) {
        return &buffer_[index];
    }

    void commitWrite(size_t count) {
        size_t write = write_pos_.load(std::memory_order_relaxed);
        write_pos_.store((write + count) & mask_, std::memory_order_release);
    }

    // Get read pointer for direct reading (consumer side)
    std::pair<size_t, size_t> prepareRead(size_t count) {
        size_t read = read_pos_.load(std::memory_order_relaxed);
        size_t write = write_pos_.load(std::memory_order_acquire);
        size_t available = (write >= read) ? (write - read) : (capacity_ - read + write);
        count = (count < available) ? count : available;

        size_t first_chunk = std::min(count, capacity_ - read);
        size_t second_chunk = count - first_chunk;
        return {first_chunk, second_chunk};
    }

    const T* getReadPtr(size_t index) const {
        return &buffer_[index];
    }

    void commitRead(size_t count) {
        size_t read = read_pos_.load(std::memory_order_relaxed);
        read_pos_.store((read + count) & mask_, std::memory_order_release);
    }

private:
    static size_t nextPowerOfTwo(size_t n) {
        if (n == 0) return 2;
        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n |= n >> 32;
        return n + 1;
    }

    const size_t capacity_;
    const size_t mask_;
    std::unique_ptr<T[]> buffer_;

    // Cache-line aligned to prevent false sharing
    alignas(64) std::atomic<size_t> write_pos_;
    alignas(64) std::atomic<size_t> read_pos_;
};

// Audio frame buffer for SPSC queue
// Stores contiguous frames of multi-channel audio
struct AudioFrame {
    static constexpr size_t MAX_CHANNELS = 2;
    float data[MAX_CHANNELS];
};

using AudioRingBuffer = SpscRingBuffer<AudioFrame>;

} // namespace rtvcc