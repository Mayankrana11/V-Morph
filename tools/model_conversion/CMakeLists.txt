# Model Conversion Tool

add_executable(rtvc_model_convert
    main.cpp
)

target_link_libraries(rtvc_model_convert PRIVATE
    rtvc_core
)