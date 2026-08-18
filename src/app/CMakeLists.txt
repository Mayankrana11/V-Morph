# App CMakeLists.txt

add_library(rtvc_app STATIC
    Application.cpp
    AppConfig.cpp
)

target_include_directories(rtvc_app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rtvc_app PRIVATE
    rtvc_audio
    rtvc_vc
    rtvc_threading
    rtvc_metrics
    nlohmann_json::nlohmann_json
)