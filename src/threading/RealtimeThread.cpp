#include "RealtimeThread.h"
#include "ThreadUtils.h"
#include <thread>
#include <atomic>
#include <chrono>

namespace rtvcc {

RealtimeThread::RealtimeThread(ThreadFunc func,
                               ThreadPriority priority,
                               const std::string& name,
                               CpuAffinityMask affinity)
    : priority_(priority)
    , name_(name) {
    if (func) {
        start(std::move(func), priority, name, affinity);
    }
}

RealtimeThread::~RealtimeThread() {
    stop();
    if (thread_.joinable()) {
        thread_.join();
    }
}

RealtimeThread::RealtimeThread(RealtimeThread&& other) noexcept
    : thread_(std::move(other.thread_))
    , stop_flag_(other.stop_flag_.load())
    , priority_(other.priority_)
    , name_(std::move(other.name_)) {
    other.stop_flag_.store(true);
}

RealtimeThread& RealtimeThread::operator=(RealtimeThread&& other) noexcept {
    if (this != &other) {
        stop();
        if (thread_.joinable()) {
            thread_.join();
        }
        thread_ = std::move(other.thread_);
        stop_flag_.store(other.stop_flag_.load());
        priority_ = other.priority_;
        name_ = std::move(other.name_);
        other.stop_flag_.store(true);
    }
    return *this;
}

bool RealtimeThread::start(ThreadFunc func,
                           ThreadPriority priority,
                           const std::string& name,
                           CpuAffinityMask affinity) {
    if (thread_.joinable()) {
        return false;
    }

    priority_ = priority;
    name_ = name;
    stop_flag_.store(false, std::memory_order_release);

    thread_ = std::thread([this, func = std::move(func)]() {
        ThreadUtils::setCurrentThreadName(name_);
        ThreadUtils::setCurrentThreadPriority(priority_);
        func(stop_flag_);
    });

    if (affinity != 0) {
        setAffinity(affinity);
    }

    return true;
}

void RealtimeThread::stop() {
    stop_flag_.store(true, std::memory_order_release);
}

void RealtimeThread::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

void RealtimeThread::detach() {
    if (thread_.joinable()) {
        thread_.detach();
    }
}

bool RealtimeThread::setPriority(ThreadPriority priority) {
    if (!thread_.joinable()) return false;
    return ThreadUtils::setCurrentThreadPriority(priority);
}

bool RealtimeThread::setAffinity(CpuAffinityMask mask) {
    if (!thread_.joinable()) return false;
    return ThreadUtils::setCurrentThreadAffinity(mask);
}

// =====================================================================
// ThreadUtils implementation
// =====================================================================

namespace ThreadUtils {

void setCurrentThreadName(const std::string& name) {
#if defined(_WIN32)
    // Windows: SetThreadDescription (Windows 10 1607+)
    using SetThreadDescriptionFunc = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    static SetThreadDescriptionFunc pSetThreadDescription = nullptr;
    if (!pSetThreadDescription) {
        HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
        pSetThreadDescription = reinterpret_cast<SetThreadDescriptionFunc>(
            GetProcAddress(kernel32, "SetThreadDescription"));
    }
    if (pSetThreadDescription) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, nullptr, 0);
        std::wstring wname(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, &wname[0], wlen);
        pSetThreadDescription(GetCurrentThread(), wname.c_str());
    }
#elif defined(__linux__)
    // Linux: pthread_setname_np
    pthread_setname_np(pthread_self(), name.substr(0, 15).c_str());
#elif defined(__APPLE__)
    // macOS: pthread_setname_np
    pthread_setname_np(name.c_str());
#endif
}

bool setCurrentThreadPriority(ThreadPriority priority) {
#if defined(_WIN32)
    int win_priority = THREAD_PRIORITY_NORMAL;
    switch (priority) {
        case ThreadPriority::Normal: win_priority = THREAD_PRIORITY_NORMAL; break;
        case ThreadPriority::High: win_priority = THREAD_PRIORITY_HIGHEST; break;
        case ThreadPriority::RealTime: win_priority = THREAD_PRIORITY_TIME_CRITICAL; break;
        case ThreadPriority::Critical: win_priority = THREAD_PRIORITY_TIME_CRITICAL; break;
    }

    // For RealTime/Critical, try to use MMCSS
    if (priority == ThreadPriority::RealTime || priority == ThreadPriority::Critical) {
        // Try to register with MMCSS Pro Audio
        typedef HANDLE(WINAPI* AvSetMmThreadCharacteristicsFunc)(LPCWSTR, LPDWORD);
        typedef BOOL(WINAPI* AvRevertMmThreadCharacteristicsFunc)(HANDLE);

        static AvSetMmThreadCharacteristicsFunc pAvSetMmThreadCharacteristics = nullptr;
        static AvRevertMmThreadCharacteristicsFunc pAvRevertMmThreadCharacteristics = nullptr;

        if (!pAvSetMmThreadCharacteristics) {
            HMODULE avrt = LoadLibraryW(L"avrt.dll");
            if (avrt) {
                pAvSetMmThreadCharacteristics = reinterpret_cast<AvSetMmThreadCharacteristicsFunc>(
                    GetProcAddress(avrt, "AvSetMmThreadCharacteristicsW"));
                pAvRevertMmThreadCharacteristics = reinterpret_cast<AvRevertMmThreadCharacteristicsFunc>(
                    GetProcAddress(avrt, "AvRevertMmThreadCharacteristics"));
            }
        }

        if (pAvSetMmThreadCharacteristics) {
            DWORD task_index = 0;
            HANDLE handle = pAvSetMmThreadCharacteristics(L"Pro Audio", &task_index);
            if (handle) {
                // Success - MMCSS registered
                return true;
            }
        }
    }

    return SetThreadPriority(GetCurrentThread(), win_priority) != 0;

#elif defined(__linux__)
    // Linux: use pthread_setschedparam with SCHED_FIFO/SCHED_RR
    // Requires CAP_SYS_NICE or running as root
    int policy = SCHED_OTHER;
    int prio = 0;

    switch (priority) {
        case ThreadPriority::Normal: policy = SCHED_OTHER; prio = 0; break;
        case ThreadPriority::High: policy = SCHED_RR; prio = 10; break;
        case ThreadPriority::RealTime: policy = SCHED_FIFO; prio = 50; break;
        case ThreadPriority::Critical: policy = SCHED_FIFO; prio = 90; break;
    }

    struct sched_param param{};
    param.sched_priority = prio;
    return pthread_setschedparam(pthread_self(), policy, &param) == 0;

#elif defined(__APPLE__)
    // macOS: use pthread_set_qos_class_self_np
    qos_class_t qos = QOS_CLASS_DEFAULT;
    switch (priority) {
        case ThreadPriority::Normal: qos = QOS_CLASS_DEFAULT; break;
        case ThreadPriority::High: qos = QOS_CLASS_USER_INITIATED; break;
        case ThreadPriority::RealTime: qos = QOS_CLASS_USER_INTERACTIVE; break;
        case ThreadPriority::Critical: qos = QOS_CLASS_USER_INTERACTIVE; break;
    }
    return pthread_set_qos_class_self_np(qos, 0) == 0;
#endif
    return false;
}

bool setCurrentThreadAffinity(CpuAffinityMask mask) {
#if defined(_WIN32)
    return SetThreadAffinityMask(GetCurrentThread(), static_cast<DWORD_PTR>(mask)) != 0;
#elif defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int i = 0; i < 64; ++i) {
        if (mask & (1ULL << i)) CPU_SET(i, &cpuset);
    }
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#else
    return false;
#endif
}

size_t getHardwareConcurrency() {
    return std::thread::hardware_concurrency();
}

std::thread::id getCurrentThreadId() {
    return std::this_thread::get_id();
}

void sleepFor(std::chrono::nanoseconds ns) {
    std::this_thread::sleep_for(ns);
}

void sleepUntil(std::chrono::steady_clock::time_point time) {
    std::this_thread::sleep_until(time);
}

void busyWait(std::chrono::nanoseconds ns) {
    auto end = std::chrono::steady_clock::now() + ns;
    while (std::chrono::steady_clock::now() < end) {
        // Spin
#if defined(_MSC_VER)
        _mm_pause();
#else
        __builtin_ia32_pause();
#endif
    }
}

} // namespace ThreadUtils

} // namespace rtvcc