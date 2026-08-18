#pragma once

#include <thread>
#include <atomic>
#include <functional>
#include <string>
#include <chrono>

namespace rtvcc {

// Real-time thread priority levels
enum class ThreadPriority {
    Normal,
    High,
    RealTime,      // MMCSS Pro Audio on Windows
    Critical       // Highest available
};

// Thread affinity mask
using CpuAffinityMask = uint64_t;

// RAII wrapper for real-time thread
class RealtimeThread {
public:
    using ThreadFunc = std::function<void(std::atomic<bool>& stop_flag)>;

    RealtimeThread() = default;
    explicit RealtimeThread(ThreadFunc func,
                           ThreadPriority priority = ThreadPriority::High,
                           const std::string& name = "RTVC_Thread",
                           CpuAffinityMask affinity = 0);
    ~RealtimeThread();

    RealtimeThread(const RealtimeThread&) = delete;
    RealtimeThread& operator=(const RealtimeThread&) = delete;
    RealtimeThread(RealtimeThread&& other) noexcept;
    RealtimeThread& operator=(RealtimeThread&& other) noexcept;

    // Start the thread
    bool start(ThreadFunc func,
               ThreadPriority priority = ThreadPriority::High,
               const std::string& name = "RTVC_Thread",
               CpuAffinityMask affinity = 0);

    // Request stop and wait for completion
    void stop();
    void join();
    void detach();

    // Check if thread is running
    bool isRunning() const { return thread_.joinable(); }
    std::thread::id getId() const { return thread_.get_id(); }

    // Platform-specific: set thread priority after start
    bool setPriority(ThreadPriority priority);

    // Platform-specific: set CPU affinity
    bool setAffinity(CpuAffinityMask mask);

    // Get native handle for platform-specific operations
    std::thread::native_handle_type nativeHandle() { return thread_.native_handle(); }

private:
    std::thread thread_;
    std::atomic<bool> stop_flag_{false};
    ThreadPriority priority_{ThreadPriority::Normal};
    std::string name_;
};

// Thread utilities
namespace ThreadUtils {

// Set current thread name (for debugging/profiling)
void setCurrentThreadName(const std::string& name);

// Set current thread priority
bool setCurrentThreadPriority(ThreadPriority priority);

// Set current thread CPU affinity
bool setCurrentThreadAffinity(CpuAffinityMask mask);

// Get number of logical CPUs
size_t getHardwareConcurrency();

// Get current thread ID
std::thread::id getCurrentThreadId();

// Sleep for specified duration (high precision)
void sleepFor(std::chrono::nanoseconds ns);
void sleepUntil(std::chrono::steady_clock::time_point time);

// Busy wait for very short durations (use sparingly)
void busyWait(std::chrono::nanoseconds ns);

} // namespace ThreadUtils

} // namespace rtvcc