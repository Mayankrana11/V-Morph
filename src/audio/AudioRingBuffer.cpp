#include "AudioRingBuffer.h"
#include <algorithm>

namespace rtvcc {

template <typename T>
SpscRingBuffer<T>::SpscRingBuffer(size_t capacity)
    : capacity_(nextPowerOfTwo(capacity))
    , mask_(capacity_ - 1)
    , buffer_(std::make_unique<T[]>(capacity_))
    , write_pos_(0)
    , read_pos_(0)
{}

template <typename T>
SpscRingBuffer<T>::~SpscRingBuffer() = default;

template <typename T>
SpscRingBuffer<T>::SpscRingBuffer(SpscRingBuffer&& other) noexcept
    : capacity_(other.capacity_)
    , mask_(other.mask_)
    , buffer_(std::move(other.buffer_))
    , write_pos_(other.write_pos_.load())
    , read_pos_(other.read_pos_.load())
{}

template <typename T>
SpscRingBuffer<T>& SpscRingBuffer<T>::operator=(SpscRingBuffer&& other) noexcept {
    if (this != &other) {
        capacity_ = other.capacity_;
        mask_ = other.mask_;
        buffer_ = std::move(other.buffer_);
        write_pos_.store(other.write_pos_.load());
        read_pos_.store(other.read_pos_.load());
    }
    return *this;
}

template <typename T>
bool SpscRingBuffer<T>::push(const T& item) {
    const size_t write = write_pos_.load(std::memory_order_relaxed);
    const size_t next_write = (write + 1) & mask_;
    if (next_write == read_pos_.load(std::memory_order_acquire)) {
        return false;
    }
    buffer_[write] = item;
    write_pos_.store(next_write, std::memory_order_release);
    return true;
}

template <typename T>
size_t SpscRingBuffer<T>::push(const T* items, size_t count) {
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

template <typename T>
bool SpscRingBuffer<T>::pop(T& item) {
    const size_t read = read_pos_.load(std::memory_order_relaxed);
    if (read == write_pos_.load(std::memory_order_acquire)) {
        return false;
    }
    item = buffer_[read];
    read_pos_.store((read + 1) & mask_, std::memory_order_release);
    return true;
}

template <typename T>
size_t SpscRingBuffer<T>::pop(T* items, size_t count) {
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

template <typename T>
size_t SpscRingBuffer<T>::peek(T* items, size_t count) const {
    size_t read = read_pos_.load(std::memory_order_relaxed);
    size_t write = write_pos_.load(std::memory_order_acquire);
    size_t available = (write >= read) ? (write - read) : (capacity_ - read + write);
    size_t to_peek = (count < available) ? count : available;

    for (size_t i = 0; i < to_peek; ++i) {
        items[i] = buffer_[(read + i) & mask_];
    }
    return to_peek;
}

template <typename T>
size_t SpscRingBuffer<T>::discard(size_t count) {
    size_t read = read_pos_.load(std::memory_order_relaxed);
    size_t write = write_pos_.load(std::memory_order_acquire);
    size_t available = (write >= read) ? (write - read) : (capacity_ - read + write);
    size_t to_discard = (count < available) ? count : available;

    read_pos_.store((read + to_discard) & mask_, std::memory_order_release);
    return to_discard;
}

template <typename T>
size_t SpscRingBuffer<T>::size() const {
    size_t write = write_pos_.load(std::memory_order_acquire);
    size_t read = read_pos_.load(std::memory_order_acquire);
    return (write >= read) ? (write - read) : (capacity_ - read + write);
}

template <typename T>
size_t SpscRingBuffer<T>::capacity() const {
    return capacity_ - 1;
}

template <typename T>
size_t SpscRingBuffer<T>::available() const {
    return capacity() - size();
}

template <typename T>
bool SpscRingBuffer<T>::empty() const {
    return read_pos_.load(std::memory_order_acquire) == write_pos_.load(std::memory_order_acquire);
}

template <typename T>
bool SpscRingBuffer<T>::full() const {
    size_t write = write_pos_.load(std::memory_order_acquire);
    size_t next_write = (write + 1) & mask_;
    return next_write == read_pos_.load(std::memory_order_acquire);
}

template <typename T>
void SpscRingBuffer<T>::clear() {
    read_pos_.store(0, std::memory_order_release);
    write_pos_.store(0, std::memory_order_release);
}

template <typename T>
std::pair<size_t, size_t> SpscRingBuffer<T>::prepareWrite(size_t count) {
    size_t write = write_pos_.load(std::memory_order_relaxed);
    size_t read = read_pos_.load(std::memory_order_acquire);
    size_t available = (read > write) ? (read - write - 1) : (capacity_ - write + read - 1);
    count = (count < available) ? count : available;

    size_t first_chunk = std::min(count, capacity_ - write);
    size_t second_chunk = count - first_chunk;
    return {first_chunk, second_chunk};
}

template <typename T>
T* SpscRingBuffer<T>::getWritePtr(size_t index) {
    return &buffer_[index];
}

template <typename T>
void SpscRingBuffer<T>::commitWrite(size_t count) {
    size_t write = write_pos_.load(std::memory_order_relaxed);
    write_pos_.store((write + count) & mask_, std::memory_order_release);
}

template <typename T>
std::pair<size_t, size_t> SpscRingBuffer<T>::prepareRead(size_t count) {
    size_t read = read_pos_.load(std::memory_order_relaxed);
    size_t write = write_pos_.load(std::memory_order_acquire);
    size_t available = (write >= read) ? (write - read) : (capacity_ - read + write);
    count = (count < available) ? count : available;

    size_t first_chunk = std::min(count, capacity_ - read);
    size_t second_chunk = count - first_chunk;
    return {first_chunk, second_chunk};
}

template <typename T>
const T* SpscRingBuffer<T>::getReadPtr(size_t index) const {
    return &buffer_[index];
}

template <typename T>
void SpscRingBuffer<T>::commitRead(size_t count) {
    size_t read = read_pos_.load(std::memory_order_relaxed);
    read_pos_.store((read + count) & mask_, std::memory_order_release);
}

template <typename T>
size_t SpscRingBuffer<T>::nextPowerOfTwo(size_t n) {
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

// Explicit instantiation for AudioFrame
template class SpscRingBuffer<AudioFrame>;

} // namespace rtvcc