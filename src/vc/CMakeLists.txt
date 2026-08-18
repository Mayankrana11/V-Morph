# Voice Converter Library

add_library(rtvc_vc STATIC
    VoiceConverter.cpp
    PassthroughConverter.cpp
    DSPVoiceConverter.cpp
    StreamingVC.cpp
)

target_include_directories(rtvc_vc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rtvc_vc PRIVATE
    rtvc_dsp
    rtvc_inference
)