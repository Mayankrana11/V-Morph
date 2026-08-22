# Tests CMakeLists.txt

# Enable testing
enable_testing()

# Test executable
add_executable(v-morph_tests
    test_audio_ringbuffer.cpp
    test_dsp.cpp
    test_voice_converter.cpp
    test_metrics.cpp
    test_threading.cpp
    test_inference.cpp
    test_onnx_converter.cpp
    test_integration.cpp
    test_performance_regression.cpp
    test_main.cpp
)

target_link_libraries(v-morph_tests PRIVATE
    rtvc_core
    GTest::gtest
    GTest::gtest_main
)

# Add test
add_test(NAME v-morph_unit_tests COMMAND v-morph_tests)

# Set test properties
set_tests_properties(v-morph_unit_tests PROPERTIES
    TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)