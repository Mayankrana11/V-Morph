# Benchmarks

add_executable(rtvc_benchmarks
    main.cpp
)

target_link_libraries(rtvc_benchmarks PRIVATE
    rtvc_core
)