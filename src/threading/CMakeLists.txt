# Threading Library

add_library(rtvc_threading STATIC
    RealtimeThread.cpp
    ThreadUtils.cpp
    SpscQueue.cpp
)

target_include_directories(rtvc_threading PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)