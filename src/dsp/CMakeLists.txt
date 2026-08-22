# DSP Library

add_library(rtvc_dsp STATIC
    Resampler.cpp
)

target_include_directories(rtvc_dsp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)