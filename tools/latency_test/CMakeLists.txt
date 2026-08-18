# Latency Test Tool

add_executable(rtvc_latency_test
    main.cpp
)

target_link_libraries(rtvc_latency_test PRIVATE
    rtvc_core
)