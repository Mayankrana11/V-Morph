# Dependency management using vcpkg and FetchContent

# vcpkg toolchain
if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "${ENV{VCPKG_ROOT}}/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${ENV{VCPKG_ROOT}}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "vcpkg toolchain")
    message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

# Find/Download dependencies
# We prefer vcpkg but fallback to FetchContent

# =====================================================================
# ONNX Runtime
# =====================================================================
option(USE_ONNXRUNTIME "Enable ONNX Runtime support" ON)
if(USE_ONNXRUNTIME)
    find_package(onnxruntime CONFIG QUIET)
    if(NOT onnxruntime_FOUND)
        # Try vcpkg
        find_package(onnxruntime REQUIRED)
    endif()
    if(onnxruntime_FOUND)
        add_compile_definitions(RTVC_HAVE_ONNXRUNTIME=1)
        message(STATUS "Found ONNX Runtime: ${onnxruntime_VERSION}")
    else()
        message(FATAL_ERROR "ONNX Runtime not found. Install via vcpkg: vcpkg install onnxruntime")
    endif()
endif()

# =====================================================================
# RtAudio (cross-platform audio I/O)
# =====================================================================
option(USE_RTAUDIO "Use RtAudio for audio I/O" ON)
if(USE_RTAUDIO)
    # Try to find system RtAudio or use FetchContent
    find_package(RtAudio QUIET)
    if(NOT RtAudio_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            rtaudio
            GIT_REPOSITORY https://github.com/thestk/rtaudio.git
            GIT_TAG        6.0.0
        )
        FetchContent_MakeAvailable(rtaudio)
        # RtAudio uses CMake, targets: rtaudio_static or rtaudio_shared
    endif()
    if(TARGET rtaudio_static OR TARGET rtaudio_shared OR RtAudio_FOUND)
        add_compile_definitions(RTVC_HAVE_RTAUDIO=1)
        message(STATUS "RtAudio available")
    else()
        message(WARNING "RtAudio not found, will use platform-specific audio backend")
    endif()
endif()

# =====================================================================
# Dear ImGui (UI)
# =====================================================================
option(USE_IMGUI "Use Dear ImGui for UI" ON)
if(USE_IMGUI)
    include(FetchContent)
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.8
    )
    FetchContent_MakeAvailable(imgui)
    add_compile_definitions(RTVC_HAVE_IMGUI=1)
    message(STATUS "Dear ImGui available")
endif()

# =====================================================================
# GoogleTest (testing)
# =====================================================================
if(BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
    )
    FetchContent_MakeAvailable(googletest)
    message(STATUS "GoogleTest available")
endif()

# =====================================================================
# Google Benchmark (benchmarking)
# =====================================================================
if(BUILD_BENCHMARKS)
    include(FetchContent)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.9.3
    )
    FetchContent_MakeAvailable(benchmark)
    message(STATUS "Google Benchmark available")
endif()

# =====================================================================
# spdlog (logging)
# =====================================================================
include(FetchContent)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
)
FetchContent_MakeAvailable(spdlog)
add_compile_definitions(RTVC_HAVE_SPDLOG=1)

# =====================================================================
# nlohmann/json (configuration)
# =====================================================================
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# =====================================================================
# Concurrentqueue (lock-free SPSC queue)
# =====================================================================
FetchContent_Declare(
    concurrentqueue
    GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
    GIT_TAG        v1.0.4
)
FetchContent_MakeAvailable(concurrentqueue)

# =====================================================================
# Thread pool / task system (optional)
# =====================================================================
FetchContent_Declare(
    taskflow
    GIT_REPOSITORY https://github.com/taskflow/taskflow.git
    GIT_TAG        v3.6.0
)
FetchContent_MakeAvailable(taskflow)

# =====================================================================
# Platform-specific audio backend selection
# =====================================================================
if(WIN32)
    add_compile_definitions(RTVC_AUDIO_BACKEND_WASAPI=1)
    # WASAPI is native, no extra dependency needed
elseif(UNIX AND NOT APPLE)
    add_compile_definitions(RTVC_AUDIO_BACKEND_ALSA=1)
    find_library(ASOUND_LIB asound)
    if(ASOUND_LIB)
        list(APPEND PLATFORM_LIBS ${ASOUND_LIB})
    endif()
elseif(APPLE)
    add_compile_definitions(RTVC_AUDIO_BACKEND_COREAUDIO=1)
endif()

# =====================================================================
# Summary
# =====================================================================
message(STATUS "")
message(STATUS "=== Dependency Summary ===")
message(STATUS "ONNX Runtime:     ${onnxruntime_FOUND}")
message(STATUS "RtAudio:          ${RtAudio_FOUND OR TARGET rtaudio_static}")
message(STATUS "Dear ImGui:       ${USE_IMGUI}")
message(STATUS "GoogleTest:       ${BUILD_TESTS}")
message(STATUS "Google Benchmark: ${BUILD_BENCHMARKS}")
message(STATUS "spdlog:           ON (FetchContent)")
message(STATUS "nlohmann/json:    ON (FetchContent)")
message(STATUS "concurrentqueue:  ON (FetchContent)")
message(STATUS "taskflow:         ON (FetchContent)")
message(STATUS "")