# UI Library

add_library(rtvc_ui STATIC
    MainWindow.cpp
    DevicePanel.cpp
    VoicePanel.cpp
    PerformancePanel.cpp
)

target_include_directories(rtvc_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rtvc_ui PRIVATE
    rtvc_core
)

if(USE_IMGUI)
    target_link_libraries(rtvc_ui PRIVATE imgui)
endif()