# Audio Library

add_library(rtvc_audio STATIC
    AudioEngine.cpp
    AudioDevice.cpp
    AudioDeviceManager.cpp
    AudioRingBuffer.cpp
    AudioClock.cpp
)

target_include_directories(rtvc_audio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rtvc_audio PUBLIC
    ${PLATFORM_LIBS}
)

if(WIN32)
    target_sources(rtvc_audio PRIVATE
        ../platform/windows/WasapiDevice.cpp
    )
endif()