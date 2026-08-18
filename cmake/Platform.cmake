# Platform-specific settings and definitions

if(WIN32)
    add_compile_definitions(RTVC_PLATFORM_WINDOWS=1)
    add_compile_definitions(NOMINMAX)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(_WIN32_WINNT=0x0A00)  # Windows 10

    # Link Windows libraries
    set(PLATFORM_LIBS
        ${PLATFORM_LIBS}
        ole32
        oleaut32
        uuid
        avrt
        mf
        mfplat
        mfuuid
        ksuser
    )

elseif(UNIX AND NOT APPLE)
    add_compile_definitions(RTVC_PLATFORM_LINUX=1)
    set(PLATFORM_LIBS
        ${PLATFORM_LIBS}
        pthread
        dl
        rt
        asound
    )

elseif(APPLE)
    add_compile_definitions(RTVC_PLATFORM_MACOS=1)
    set(PLATFORM_LIBS
        ${PLATFORM_LIBS}
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework CoreFoundation"
        "-framework Foundation"
    )
endif()

# Architecture detection
if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|AMD64)")
    add_compile_definitions(RTVC_ARCH_X64=1)
    set(RTVC_ARCH "x64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "(aarch64|arm64)")
    add_compile_definitions(RTVC_ARCH_ARM64=1)
    set(RTVC_ARCH "arm64")
else()
    message(WARNING "Unknown architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# SIMD support detection
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-mavx2" HAS_AVX2_FLAG)
check_cxx_compiler_flag("/arch:AVX2" HAS_AVX2_FLAG_MSVC)
if(HAS_AVX2_FLAG OR HAS_AVX2_FLAG_MSVC)
    add_compile_definitions(RTVC_HAVE_AVX2=1)
endif()

check_cxx_compiler_flag("-mfma" HAS_FMA_FLAG)
check_cxx_compiler_flag("/arch:AVX2" HAS_FMA_FLAG_MSVC)
if(HAS_FMA_FLAG OR HAS_FMA_FLAG_MSVC)
    add_compile_definitions(RTVC_HAVE_FMA=1)
endif()