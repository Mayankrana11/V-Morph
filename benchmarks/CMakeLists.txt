# Benchmarks

add_executable(v-morph_benchmarks
    main.cpp
)

target_link_libraries(v-morph_benchmarks PRIVATE
    rtvc_core
)