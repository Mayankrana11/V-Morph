# Diagnostics Tool

add_executable(v-morph_diagnostics
    main.cpp
)

target_link_libraries(v-morph_diagnostics PRIVATE
    rtvc_core
)