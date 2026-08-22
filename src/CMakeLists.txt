# Main source directory CMakeLists.txt

# Audio library
add_subdirectory(audio)

# DSP library
add_subdirectory(dsp)

# Inference library
add_subdirectory(inference)

# Voice Converter library
add_subdirectory(vc)

# Threading library
add_subdirectory(threading)

# Metrics library
add_subdirectory(metrics)

# App library
add_subdirectory(app)

# UI library
add_subdirectory(ui)

# Core library combining all
add_library(rtvc_core STATIC)

target_link_libraries(rtvc_core PUBLIC
    rtvc_audio
    rtvc_dsp
    rtvc_inference
    rtvc_vc
    rtvc_threading
    rtvc_metrics
    rtvc_app
)

target_include_directories(rtvc_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(rtvc_core PUBLIC
    ${PLATFORM_LIBS}
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    concurrentqueue::concurrentqueue
    taskflow::taskflow
)

if(USE_ONNXRUNTIME)
    target_link_libraries(rtvc_core PUBLIC onnxruntime)
endif()

if(USE_RTAUDIO AND TARGET rtaudio_static)
    target_link_libraries(rtvc_core PUBLIC rtaudio_static)
elseif(USE_RTAUDIO AND RtAudio_FOUND)
    target_link_libraries(rtvc_core PUBLIC RtAudio::RtAudio)
endif()

if(USE_IMGUI)
    target_link_libraries(rtvc_core PUBLIC imgui)
endif()

# Platform-specific sources
if(WIN32)
    target_sources(rtvc_core PRIVATE
        platform/windows/WasapiDevice.cpp
        platform/windows/VirtualAudio.cpp
    )
endif()

# Main executable
add_executable(v-morph main.cpp)
target_link_libraries(v-morph PRIVATE rtvc_core)

if(BUILD_TESTS)
    target_link_libraries(v-morph PRIVATE GTest::gtest GTest::gtest_main)
endif()

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/v-morph
    FILES_MATCHING PATTERN "*.h"
)