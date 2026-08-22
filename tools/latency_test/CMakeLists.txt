# Latency Test Tool

add_executable(v-morph_latency_test
    main.cpp
)

target_link_libraries(v-morph_latency_test PRIVATE
    rtvc_core
)