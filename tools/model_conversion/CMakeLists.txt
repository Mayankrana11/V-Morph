# Model Conversion Tool

add_executable(v-morph_model_convert
    main.cpp
)

target_link_libraries(v-morph_model_convert PRIVATE
    rtvc_core
)