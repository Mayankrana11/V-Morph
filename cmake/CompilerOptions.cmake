# Compiler warning and optimization settings

if(MSVC)
    # MSVC warning levels
    add_compile_options(
        /W4
        /permissive-
        /Zc:__cplusplus
        /Zc:inline
        /Zc:throwingNew
        /Zc:strictStrings
        /Zc:rvalueCast
        /guard:cf
    )

    # Disable specific noisy warnings
    add_compile_options(
        /wd4100   # unreferenced formal parameter
        /wd4201   # nameless struct/union
        /wd4324   # structure padded due to alignment
        /wd4514   # unreferenced inline function removed
        /wd4710   # function not inlined
        /wd4711   # function selected for automatic inlining
        /wd4820   # padding added
        /wd5045   # Spectre mitigation
        /wd5220   # non-standard syntax
    )

    # Debug vs Release
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(/Od /RTC1 /MDd /Zi /Ob0)
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /bigobj")
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2 /Oi /GL /MD /Gy /fp:fast)
        add_link_options(/LTCG)
    elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_options(/O2 /Oi /GL /MD /Zi /fp:fast)
        add_link_options(/LTCG /DEBUG)
    elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        add_compile_options(/O1 /Oi /GL /MD /fp:fast)
        add_link_options(/LTCG)
    endif()

else()
    # GCC/Clang
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )

    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(
            -Wno-zero-as-null-pointer-constant
            -Wno-old-style-cast
        )
    endif()

    # Debug vs Release
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-O0 -g3 -fno-omit-frame-pointer -fsanitize=address,undefined)
        add_link_options(-fsanitize=address,undefined)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -DNDEBUG -ffast-math -fno-finite-math-only)
        add_link_options(-s)
    elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_options(-O2 -g -DNDEBUG -ffast-math -fno-finite-math-only)
    elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        add_compile_options(-Os -DNDEBUG -ffast-math -fno-finite-math-only)
        add_link_options(-s)
    endif()

    # Link time optimization
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        add_compile_options(-flto=auto)
        add_link_options(-flto=auto)
    endif()
endif()

# Architecture-specific flags for x64
if(RTVC_ARCH_X64)
    if(MSVC)
        add_compile_options(/arch:AVX2)
    else()
        add_compile_options(-mavx2 -mfma)
    endif()
endif()

# Security hardening
if(MSVC)
    add_compile_options(/DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA)
    add_link_options(/DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA)
else()
    add_link_options(-Wl,-z,relro,-z,now)
    if(NOT APPLE)
        add_link_options(-Wl,-z,noexecstack)
    endif()
endif()

# Real-time safety: no exceptions in audio callback path
# (enforced by code convention, not compiler flag)