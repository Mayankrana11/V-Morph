# Diagnostics Tool

add_executable(rtvc_diagnostics
    main.cpp
)

target_link_libraries(rtvc_diagnostics PRIVATE
    rtvc_core
)