# Blosc - Blocked Shuffling and Compression Library
#
# Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
# https://blosc.org
# License: BSD 3-Clause (see LICENSE.txt)
#
# See LICENSE.txt for details about copyright and rights to use.

# A simple way to detect that we are using CMAKE
add_definitions(-DUSING_CMAKE)

set(version_string ${BLOSC2_VERSION_MAJOR}.${BLOSC2_VERSION_MINOR}.${BLOSC2_VERSION_PATCH})

# targets
if(BUILD_SHARED)
    add_library(blosc2_shared SHARED)
    # ALIAS for superbuilds that use Blosc2 as sub-project
    # must be the same as the NAMESPACE in Blosc2Targets
    add_library(Blosc2::blosc2_shared ALIAS blosc2_shared)
    set_target_properties(blosc2_shared PROPERTIES
        OUTPUT_NAME blosc2
        # Hide symbols by default unless they're specifically exported.
        # This makes it easier to keep the set of exported symbols the
        # same across all compilers/platforms.
        C_VISIBILITY_PRESET hidden
    )
    if(MSVC OR MINGW)
        set_target_properties(blosc2_shared PROPERTIES PREFIX lib)
    endif()
    set_target_properties(blosc2_shared PROPERTIES
        VERSION ${version_string}
        SOVERSION 4  # Change this when an ABI change happens
    )
    target_compile_definitions(blosc2_shared PUBLIC BLOSC_SHARED_LIBRARY)
    target_include_directories(blosc2_shared PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>)
endif()
if(BUILD_STATIC)
    add_library(blosc2_static STATIC)
    # ALIAS for superbuilds that use Blosc2 as sub-project
    # must be the same as the NAMESPACE in Blosc2Targets
    add_library(Blosc2::blosc2_static ALIAS blosc2_static)
    set_target_properties(blosc2_static PROPERTIES
        OUTPUT_NAME blosc2
        POSITION_INDEPENDENT_CODE ON
        # Hide symbols by default unless they're specifically exported.
        # This makes it easier to keep the set of exported symbols the
        # same across all compilers/platforms.
        C_VISIBILITY_PRESET hidden
    )
    if(MSVC OR MINGW)
        set_target_properties(blosc2_static PROPERTIES PREFIX lib)
    endif()
    target_include_directories(blosc2_static PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>)
endif()
# When the option has been selected to compile the test suite,
# compile an additional version of blosc2_static which exports
# some normally-hidden symbols (to facilitate unit testing).
if(BUILD_TESTS)
    add_library(blosc_testing STATIC)
    set_target_properties(blosc_testing PROPERTIES OUTPUT_NAME blosc_testing)
    if(MSVC OR MINGW)
        set_target_properties(blosc_testing PROPERTIES PREFIX lib)
    endif()
    target_compile_definitions(blosc_testing PUBLIC
        BLOSC_TESTING
        BLOSC_SHARED_LIBRARY  # for EXPORT macro
    )
    target_include_directories(blosc_testing PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>)
endif()

set(INTERNAL_LIBS ${PROJECT_SOURCE_DIR}/internal-complibs)

# link dependencies
#   "link" dependent targets via target_link_libraries (preferred) and
#   manually add includes / libs for others
if(LZ4_FOUND)
    if(BUILD_SHARED)
        target_include_directories(blosc2_shared PUBLIC ${LZ4_INCLUDE_DIR})
    endif()
    if(BUILD_STATIC)
        target_include_directories(blosc2_static PUBLIC ${LZ4_INCLUDE_DIR})
    endif()
    if(BUILD_TESTS)
        target_include_directories(blosc_testing PUBLIC ${LZ4_INCLUDE_DIR})
    endif()
else()
    set(LZ4_LOCAL_DIR ${INTERNAL_LIBS}/lz4-1.10.0)
    if(BUILD_SHARED)
        target_include_directories(blosc2_shared PRIVATE ${LZ4_LOCAL_DIR})
    endif()
    if(BUILD_STATIC)
        target_include_directories(blosc2_static PRIVATE ${LZ4_LOCAL_DIR})
    endif()
    if(BUILD_TESTS)
        target_include_directories(blosc_testing PRIVATE ${LZ4_LOCAL_DIR})
    endif()
endif()

if(NOT DEACTIVATE_ZLIB)
    if(ZLIB_NG_FOUND)
        if(BUILD_SHARED)
            target_link_libraries(blosc2_shared PUBLIC ZLIB_NG::ZLIB_NG)
        endif()
        if(BUILD_STATIC)
            target_link_libraries(blosc2_static PUBLIC ZLIB_NG::ZLIB_NG)
        endif()
        if(BUILD_TESTS)
            target_link_libraries(blosc_testing PUBLIC ZLIB_NG::ZLIB_NG)
        endif()
    elseif(ZLIB_FOUND)
        if(BUILD_SHARED)
            target_link_libraries(blosc2_shared PUBLIC ZLIB::ZLIB)
        endif()
        if(BUILD_STATIC)
            target_link_libraries(blosc2_static PUBLIC ZLIB::ZLIB)
        endif()
        if(BUILD_TESTS)
            target_link_libraries(blosc_testing PUBLIC ZLIB::ZLIB)
        endif()
    else()
        set(ZLIB_LOCAL_DIR ${INTERNAL_LIBS}/${ZLIB_NG_DIR})
        if(BUILD_SHARED)
            target_include_directories(blosc2_shared PRIVATE ${ZLIB_LOCAL_DIR})
        endif()
        if(BUILD_STATIC)
            target_include_directories(blosc2_static PRIVATE ${ZLIB_LOCAL_DIR})
        endif()
        if(BUILD_TESTS)
            target_include_directories(blosc_testing PRIVATE ${ZLIB_LOCAL_DIR})
        endif()
    endif()
endif()

if(NOT DEACTIVATE_ZSTD)
    if(ZSTD_FOUND)
        if(BUILD_SHARED)
            target_include_directories(blosc2_shared PUBLIC ${ZSTD_INCLUDE_DIR})
            target_link_libraries(blosc2_shared PUBLIC ${ZSTD_LIBRARY})
        endif()
        if(BUILD_STATIC)
            target_include_directories(blosc2_static PUBLIC ${ZSTD_INCLUDE_DIR})
            target_link_libraries(blosc2_static PUBLIC ${ZSTD_LIBRARY})
        endif()
        if(BUILD_TESTS)
            target_include_directories(blosc_testing PUBLIC ${ZSTD_INCLUDE_DIR})
            target_link_libraries(blosc_testing PUBLIC ${ZSTD_LIBRARY})
        endif()
    else()
        set(ZSTD_LOCAL_DIR ${INTERNAL_LIBS}/zstd-1.5.6)
        if(BUILD_SHARED)
            target_include_directories(blosc2_shared PRIVATE ${ZSTD_LOCAL_DIR} ${ZSTD_LOCAL_DIR}/common)
        endif()
        if(BUILD_STATIC)
            target_include_directories(blosc2_static PRIVATE ${ZSTD_LOCAL_DIR} ${ZSTD_LOCAL_DIR}/common)
        endif()
        if(BUILD_TESTS)
            target_include_directories(blosc_testing PRIVATE ${ZSTD_LOCAL_DIR} ${ZSTD_LOCAL_DIR}/common)
        endif()
    endif()
endif()

# Threads
if(HAVE_THREADS)
    if(CMAKE_VERSION VERSION_LESS 3.1)
        set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
    else()
        set(LIBS ${LIBS} Threads::Threads)
    endif()
elseif(WIN32)
    message(STATUS "using the internal pthread library for win32 systems.")
    list(APPEND SOURCES blosc/win32/pthread.c)
else()
    message(FATAL_ERROR "Threads required but not found.")
endif()

# dlopen/dlclose
if(NOT WIN32)
    set(LIBS ${LIBS} ${CMAKE_DL_LIBS})
endif()

if(LZ4_FOUND)
    set(LIBS ${LIBS} ${LZ4_LIBRARY})
else()
    file(GLOB LZ4_FILES ${LZ4_LOCAL_DIR}/*.c)
    list(APPEND SOURCES ${LZ4_FILES})
    source_group("LZ4" FILES ${LZ4_FILES})
endif()

if(NOT DEACTIVATE_ZLIB)
    if(ZLIB_NG_FOUND)
        set(LIBS ${LIBS} ${ZLIB_NG_LIBRARY})
    elseif(ZLIB_FOUND)
        set(LIBS ${LIBS} ${ZLIB_LIBRARIES})
    else()
        set(ZLIB_LOCAL_DIR ${INTERNAL_LIBS}/${ZLIB_NG_DIR})
        file(GLOB ZLIB_FILES ${ZLIB_LOCAL_DIR}/*.c)
        list(APPEND SOURCES ${ZLIB_FILES})
        source_group("Zlib" FILES ${ZLIB_FILES})
    endif()
endif()

if(NOT DEACTIVATE_ZSTD)
    if(ZSTD_FOUND)
        set(LIBS ${LIBS} ${ZSTD_LIBRARY})
    else()
        # Enable assembly code only when not using MSVC *and* x86 is there
        if((NOT MSVC) AND COMPILER_SUPPORT_SSE2)   # if SSE2 is here, this is an x86 platform
            message(STATUS "Adding support for assembly sources in ZSTD")
            file(GLOB ZSTD_DECOMPRESS_FILES ${ZSTD_LOCAL_DIR}/decompress/*.S)
        else()
            message(STATUS "Disabling support for assembly sources in ZSTD")
            add_compile_definitions("ZSTD_DISABLE_ASM")
        endif()
        file(GLOB ZSTD_DECOMPRESS_FILES ${ZSTD_DECOMPRESS_FILES}
                ${ZSTD_LOCAL_DIR}/decompress/*.c)
        file(GLOB ZSTD_COMMON_FILES ${ZSTD_LOCAL_DIR}/common/*.c)
        file(GLOB ZSTD_COMPRESS_FILES ${ZSTD_LOCAL_DIR}/compress/*.c)
        file(GLOB ZSTD_DICT_FILES ${ZSTD_LOCAL_DIR}/dictBuilder/*.c)
        set(ZSTD_FILES ${ZSTD_COMMON_FILES} ${ZSTD_COMPRESS_FILES}
            ${ZSTD_DECOMPRESS_FILES} ${ZSTD_DICT_FILES})
        list(APPEND SOURCES ${ZSTD_FILES})
        source_group("Zstd" FILES ${ZSTD_FILES})
    endif()
endif()

if(HAVE_IPP)
    set(LIBS ${LIBS} "${IPP_LIBRARIES}")
endif()

if(UNIX AND NOT APPLE)
    set(LIBS ${LIBS} "rt")
    set(LIBS ${LIBS} "m")
    # set(LIBS ${LIBS} "profiler")
endif()


# Blosc2 library source files
list(APPEND SOURCES
    blosc/blosc2.c
    blosc/blosclz.c
    blosc/fastcopy.c
    blosc/fastcopy.h
    blosc/schunk.c
    blosc/frame.c
    blosc/stune.c
    blosc/stune.h
    blosc/context.h
    blosc/delta.c
    blosc/delta.h
    blosc/shuffle-generic.c
    blosc/bitshuffle-generic.c
    blosc/trunc-prec.c
    blosc/trunc-prec.h
    blosc/timestamp.c
    blosc/sframe.c
    blosc/directories.c
    blosc/blosc2-stdio.c
    blosc/b2nd.c
    blosc/b2nd_utils.c
)
if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL arm64)
    if(COMPILER_SUPPORT_SSE2)
        message(STATUS "Adding run-time support for SSE2")
        list(APPEND SOURCES blosc/shuffle-sse2.c blosc/bitshuffle-sse2.c)
    endif()
    if(COMPILER_SUPPORT_AVX2)
        message(STATUS "Adding run-time support for AVX2")
        list(APPEND SOURCES blosc/shuffle-avx2.c blosc/bitshuffle-avx2.c)
    endif()
    if(COMPILER_SUPPORT_AVX512)
        message(STATUS "Adding run-time support for AVX512")
        list(APPEND SOURCES blosc/bitshuffle-avx512.c)
    endif()
endif()
if(COMPILER_SUPPORT_NEON)
    message(STATUS "Adding run-time support for NEON")
    # bitshuffle-neon.c does not offer better speed than generic on arm64 (Mac M1).
    # Besides, it does not compile on raspberry pi (armv7l), so disable it.
    list(APPEND SOURCES blosc/shuffle-neon.c)  # blosc/bitshuffle-neon.c)
endif()
if(COMPILER_SUPPORT_ALTIVEC)
    message(STATUS "Adding run-time support for ALTIVEC")
    list(APPEND SOURCES blosc/shuffle-altivec.c blosc/bitshuffle-altivec.c)
endif()
list(APPEND SOURCES blosc/shuffle.c)

# Based on the target architecture and hardware features supported
# by the C compiler, set hardware architecture optimization flags
# for specific shuffle implementations.
if(COMPILER_SUPPORT_SSE2)
    if(MSVC)
        # MSVC targets SSE2 by default on 64-bit configurations, but not 32-bit configurations.
        if(${CMAKE_SIZEOF_VOID_P} EQUAL 4)
            set_source_files_properties(
                    shuffle-sse2.c bitshuffle-sse2.c blosclz.c fastcopy.c
                    PROPERTIES COMPILE_OPTIONS "/arch:SSE2")
        endif()
    else()
        set_source_files_properties(
                shuffle-sse2.c bitshuffle-sse2.c blosclz.c fastcopy.c
                PROPERTIES COMPILE_OPTIONS -msse2)
        # Add SIMD flags for the bytedelta filter and Intel (it seems that ARM64 does not need these)
        set_source_files_properties(
                ${PROJECT_SOURCE_DIR}/plugins/filters/bytedelta/bytedelta.c
                PROPERTIES COMPILE_OPTIONS "-mssse3")
    endif()

    # Define a symbol for the shuffle-dispatch implementation
    # so it knows SSE2 is supported even though that file is
    # compiled without SSE2 support (for portability).
    set_property(
            SOURCE shuffle.c
            APPEND PROPERTY COMPILE_DEFINITIONS SHUFFLE_SSE2_ENABLED)

endif()
if(COMPILER_SUPPORT_AVX2)
    if(MSVC)
        set_source_files_properties(
                shuffle-avx2.c bitshuffle-avx2.c
                PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
    else()
        set_source_files_properties(
                shuffle-avx2.c bitshuffle-avx2.c
                PROPERTIES COMPILE_OPTIONS -mavx2)
    endif()

    # Define a symbol for the shuffle-dispatch implementation
    # so it knows AVX2 is supported even though that file is
    # compiled without AVX2 support (for portability).
    set_property(
            SOURCE shuffle.c
            APPEND PROPERTY COMPILE_DEFINITIONS SHUFFLE_AVX2_ENABLED)
endif()
if(COMPILER_SUPPORT_AVX512)
    if(MSVC)
        set_source_files_properties(
                bitshuffle-avx512.c
		PROPERTIES COMPILE_OPTIONS "/arch:AVX512")
    else()
        set_source_files_properties(
                bitshuffle-avx512.c
                PROPERTIES COMPILE_OPTIONS "-mavx512f;-mavx512bw")
    endif()

    # Define a symbol for the shuffle-dispatch implementation
    # so it knows AVX512 is supported even though that file is
    # compiled without AVX512 support (for portability).
    set_property(
            SOURCE shuffle.c
            APPEND PROPERTY COMPILE_DEFINITIONS SHUFFLE_AVX512_ENABLED)
endif()
if(COMPILER_SUPPORT_NEON)
    set_source_files_properties(
            shuffle-neon.c bitshuffle-neon.c
            PROPERTIES COMPILE_OPTIONS "-flax-vector-conversions")
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL armv7l)
        # Only armv7l needs special -mfpu=neon flag; aarch64 doesn't.
      set_source_files_properties(
            shuffle-neon.c bitshuffle-neon.c
            PROPERTIES COMPILE_OPTIONS "-mfpu=neon;-flax-vector-conversions")
    endif()
    # Define a symbol for the shuffle-dispatch implementation
    # so it knows NEON is supported even though that file is
    # compiled without NEON support (for portability).
    set_property(
            SOURCE shuffle.c
            APPEND PROPERTY COMPILE_DEFINITIONS SHUFFLE_NEON_ENABLED)
endif()
if(COMPILER_SUPPORT_ALTIVEC)
    set_source_files_properties(
            shuffle-altivec.c bitshuffle-altivec.c
            PROPERTIES COMPILE_OPTIONS -DNO_WARN_X86_INTRINSICS)
    set_property(
            SOURCE shuffle.c
            APPEND PROPERTY COMPILE_OPTIONS -DNO_WARN_X86_INTRINSICS)

    # Define a symbol for the shuffle-dispatch implementation
    # so it knows ALTIVEC is supported even though that file is
    # compiled without ALTIVEC support (for portability).
    set_property(
            SOURCE shuffle.c
            APPEND PROPERTY COMPILE_DEFINITIONS SHUFFLE_ALTIVEC_ENABLED)
endif()

# add libraries for dependencies that are not CMake targets
if(BUILD_SHARED)
    target_link_libraries(blosc2_shared PUBLIC ${LIBS})
    target_include_directories(blosc2_shared PUBLIC ${BLOSC_INCLUDE_DIRS})
endif()

if(BUILD_STATIC)
    target_link_libraries(blosc2_static PUBLIC ${LIBS})
    target_include_directories(blosc2_static PUBLIC ${BLOSC_INCLUDE_DIRS})
endif()

if(BUILD_TESTS)
    target_link_libraries(blosc_testing PUBLIC ${LIBS})
    target_include_directories(blosc_testing PUBLIC ${BLOSC_INCLUDE_DIRS})
endif()

# we use this variable in the CMake file one directory above ours
set(SOURCES ${SOURCES} PARENT_SCOPE)
