# Inference Library

add_library(rtvc_inference STATIC
    InferenceEngine.cpp
    OnnxInferenceEngine.cpp
    TensorUtils.cpp
)

target_include_directories(rtvc_inference PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rtvc_inference PRIVATE
    rtvc_dsp
)

if(USE_ONNXRUNTIME)
    target_link_libraries(rtvc_inference PRIVATE onnxruntime)
endif()