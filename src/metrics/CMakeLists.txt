# Metrics Library

add_library(rtvc_metrics STATIC
    LatencyMetrics.cpp
    PerformanceMonitor.cpp
)

target_include_directories(rtvc_metrics PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)