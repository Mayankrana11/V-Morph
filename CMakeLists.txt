cmake_minimum_required(VERSION 3.25)

project(v-morph
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "V-Morph - Real-time AI Voice Changer"
)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Policy settings for modern CMake
cmake_policy(SET CMP0077 NEW)  # option() honors normal variables
cmake_policy(SET CMP0091 NEW)  # FindPython3 prefers shared libs

# Default build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Platform detection
include(cmake/Platform.cmake)
include(cmake/CompilerOptions.cmake)
include(cmake/Dependencies.cmake)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Main targets
add_subdirectory(src)

# Tests
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Benchmarks
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Tools
add_subdirectory(tools)

# Install
include(GNUInstallDirs)
install(TARGETS v-morph
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES configs/default.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/v-morph
)
install(DIRECTORY docs/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/v-morph/docs
    FILES_MATCHING PATTERN "*.md"
)
install(FILES LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/v-morph
)

# CPack for packaging
set(CPACK_PACKAGE_NAME "V-Morph")
set(CPACK_PACKAGE_VENDOR "Mayank Rana")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Real-time AI Voice Changer")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-windows-x64")

set(CPACK_GENERATOR "ZIP")
set(CPACK_ZIP_PACKAGE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-windows-x64")

# NSIS installer (optional)
if(CPACK_NSIS_INSTALLER)
    set(CPACK_NSIS_INSTALL_ROOT "V-Morph")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_DISPLAY_NAME "V-Morph ${PROJECT_VERSION}")
    set(CPACK_NSIS_HELP_LINK "https://github.com/yourusername/v-morph")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/yourusername/v-morph")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin/v-morph.exe")
    set(CPACK_NSIS_MENU_LINKS
        "bin/v-morph.exe" "V-Morph"
        "docs/README.md" "Documentation"
    )
endif()

include(CPack)