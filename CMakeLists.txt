# CMake build system for Blosc
# ============================
#
# Available options:
#
#   BUILD_STATIC: default ON
#       build the static version of the Blosc library
#   BUILD_SHARED: default ON
#       build the shared library version of the Blosc library
#   BUILD_TESTS: default ON
#       build test programs and generates the "test" target
#   BUILD_FUZZERS: default ON
#       build fuzz programs and generates "fuzz" targets
#   BUILD_BENCHMARKS: default ON
#       build the benchmark programs
#   BUILD_EXAMPLES: default ON
#       build the examples programs
#   BUILD_PLUGINS: default ON
#       build the plugins programs
#   BUILD_LITE: default OFF
#       build a lite version (only with BloscLZ and LZ4/LZ4HC) of the blosc library
#   DEACTIVATE_AVX2: default OFF
#       do not attempt to build with AVX2 instructions
#   DEACTIVATE_ZLIB: default OFF
#       do not include support for the Zlib library
#   DEACTIVATE_ZSTD: default OFF
#       do not include support for the Zstd library
#   PREFER_EXTERNAL_LZ4: default OFF
#       when found, use the installed LZ4 libs instead of included
#       sources
#   PREFER_EXTERNAL_ZLIB: default OFF
#       when found, use the installed ZLIB libs instead of included
#       MINIZ sources
#   PREFER_EXTERNAL_ZSTD: default OFF
#       when found, use the installed ZSTD libs instead of included
#       sources
#   TEST_INCLUDE_BENCH_SHUFFLE_1: default ON
#       add a test that runs the benchmark program passing "shuffle" with 1
#       thread as second parameter
#   TEST_INCLUDE_BENCH_SHUFFLE_N: default ON
#       add a test that runs the benchmark program passing "shuffle" with all
#       threads as second parameter
#   TEST_INCLUDE_BENCH_BITSHUFFLE_1: default ON
#       add a test that runs the benchmark program passing "bitshuffle" with 1
#       thread as second parameter
#   TEST_INCLUDE_BENCH_BITSHUFFLE_N: default ON
#       add a test that runs the benchmark program passing "bitshuffle" with
#       all threads as second parameter
#   TEST_INCLUDE_BENCH_SUITE: default OFF
#       add a test that runs the benchmark program passing "suite"
#       as first parameter
#   TEST_INCLUDE_BENCH_HARDSUITE: default OFF
#       add a test that runs the benchmark program passing "hardsuite"
#       as first parameter
#   TEST_INCLUDE_BENCH_EXTREMESUITE: default OFF
#       add a test that runs the benchmark program passing "extremesuite"
#       as first parameter
#   TEST_INCLUDE_BENCH_DEBUGSUITE: default OFF
#       add a test that runs the benchmark program passing "debugsuite"
#       as first parameter
#
# Components:
#
#    LIB: includes blosc2.so
#    DEV: static includes blosc2.a and blosc2.h

if(NOT WIN32)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
endif()

cmake_minimum_required(VERSION 3.16.3)
if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} VERSION_GREATER 3.4)
  cmake_policy(SET CMP0063 NEW)
endif()
project(blosc LANGUAGES C)

# parse the full version numbers from blosc.h
file(READ include/blosc2.h _blosc_h_contents)
string(REGEX REPLACE ".*#define[ \t]+BLOSC_VERSION_MAJOR[ \t]+([0-9]+).*"
    "\\1" BLOSC_VERSION_MAJOR ${_blosc_h_contents})
string(REGEX REPLACE ".*#define[ \t]+BLOSC_VERSION_MINOR[ \t]+([0-9]+).*"
    "\\1" BLOSC_VERSION_MINOR ${_blosc_h_contents})
string(REGEX REPLACE ".*#define[ \t]+BLOSC_VERSION_RELEASE[ \t]+([0-9]+).*"
    "\\1" BLOSC_VERSION_PATCH ${_blosc_h_contents})
string(REGEX REPLACE ".*#define[ \t]+BLOSC_VERSION_STRING[ \t]+\"([-0-9A-Za-z.]+)\".*"
    "\\1" BLOSC_VERSION_STRING ${_blosc_h_contents})

message("Configuring for Blosc version: " ${BLOSC_VERSION_STRING})

# options
option(BUILD_STATIC
    "Build a static version of the blosc library." ON)
option(BUILD_SHARED
    "Build a shared library version of the blosc library." ON)
option(BUILD_TESTS
    "Build test programs form the blosc compression library" ON)
option(BUILD_FUZZERS
    "Build fuzzer programs from the blosc compression library" ${BUILD_STATIC})
option(BUILD_BENCHMARKS
    "Build benchmark programs form the blosc compression library" ON)
option(BUILD_EXAMPLES
    "Build example programs form the blosc compression library" ON)
option(BUILD_PLUGINS
    "Build plugins programs form the blosc compression library" ON)
option(BUILD_LITE
    "Build a lite version (only with BloscLZ and LZ4/LZ4HC) of the blosc library." OFF)
option(DEACTIVATE_AVX2
    "Do not attempt to build with AVX2 instructions" OFF)
option(DEACTIVATE_ZLIB
    "Do not include support for the ZLIB library." OFF)
option(DEACTIVATE_ZSTD
    "Do not include support for the ZSTD library." OFF)
option(DEACTIVATE_IPP
    "Do not include support for the Intel IPP library." ON)
option(PREFER_EXTERNAL_LZ4
    "Find and use external LZ4 library instead of included sources." OFF)
option(PREFER_EXTERNAL_ZLIB
    "Find and use external ZLIB library instead of included sources." OFF)
option(PREFER_EXTERNAL_ZSTD
    "Find and use external ZSTD library instead of included sources." OFF)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

if(MINGW)
    # This gets us DLL resource information when compiling on MinGW.
    if(NOT CMAKE_RC_COMPILER)
        set(CMAKE_RC_COMPILER windres.exe)
    endif()
endif(MINGW)

if(ENABLE_ASAN)
    message(STATUS "Enabling ASAN")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Os -fno-omit-frame-pointer -fsanitize=address")
endif()

if(BUILD_PLUGINS AND BUILD_LITE)
    message(FATAL_ERROR "Both plugins and lite Blosc versions can not be built at the same time.")
endif()

if(BUILD_LITE)
    set(DEACTIVATE_ZLIB ON)
    set(DEACTIVATE_ZSTD ON)
endif()

if(PREFER_EXTERNAL_LZ4)
    find_package(LZ4)
else()
    message(STATUS "Using LZ4 internal sources.")
endif()

if(NOT DEACTIVATE_ZLIB)
    # import the ZLIB_ROOT environment variable to help finding the zlib library
    if(PREFER_EXTERNAL_ZLIB)
        set(ZLIB_ROOT $ENV{ZLIB_ROOT})
        find_package(ZLIB)
        if(NOT ZLIB_FOUND)
            message(STATUS "No ZLIB found.  Using ZLIB-NG internal sources.")
            set(HAVE_ZLIB_NG TRUE)
        endif()
    else()
        message(STATUS "Using ZLIB-NG internal sources for ZLIB support.")
        set(HAVE_ZLIB_NG TRUE)
    endif()
    # HAVE_ZLIB will be set to true because even if the library is not found,
    # we will use the included sources for it
    set(HAVE_ZLIB TRUE)
endif()

if(HAVE_ZLIB_NG)
    set(ZLIB_NG_DIR "zlib-ng-2.0.5")  # update to the actual included version
    add_definitions(-DZLIB_COMPAT)
    set(ZLIB_COMPAT TRUE)
    set(SKIP_INSTALL_ALL TRUE)
    set(BUILD_SHARED_LIBS FALSE)
    set(ZLIB_ENABLE_TESTS OFF)
    add_subdirectory("internal-complibs/${ZLIB_NG_DIR}")

    file(COPY
            ${CMAKE_CURRENT_BINARY_DIR}/internal-complibs/${ZLIB_NG_DIR}/zconf.h
            DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/internal-complibs/${ZLIB_NG_DIR}/)
endif()

if(NOT DEACTIVATE_ZSTD)
    if(PREFER_EXTERNAL_ZSTD)
        find_package(ZSTD)
        if(NOT ZSTD_FOUND)
          message(STATUS "No ZSTD library found.  Using internal sources.")
        endif()
    else()
        message(STATUS "Using ZSTD internal sources.")
    endif()
    # HAVE_ZSTD will be set to true because even if the library is
    # not found, we will use the included sources for it
    set(HAVE_ZSTD TRUE)
endif()

if(NOT DEACTIVATE_IPP)
    find_package(IPP)
    if(IPP_FOUND)
        message(STATUS "Using IPP accelerated compression.")
        set(HAVE_IPP TRUE)
    else()
        message(STATUS "Not using IPP accelerated compression.")
        set(HAVE_IPP FALSE)
    endif()
endif()

if(BUILD_PLUGINS)
    set(HAVE_PLUGINS TRUE)
endif()

# create the config.h file
configure_file("${PROJECT_SOURCE_DIR}/blosc/config.h.in"
               "${PROJECT_SOURCE_DIR}/blosc/config.h")

# now make sure that you set the build directory on your "Include" path when compiling
include_directories("${PROJECT_BINARY_DIR}/blosc/")

# If the build type is not set, default to Release.
set(BLOSC_DEFAULT_BUILD_TYPE Release)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "No build type specified. Defaulting to '${BLOSC_DEFAULT_BUILD_TYPE}'.")
    set(CMAKE_BUILD_TYPE ${BLOSC_DEFAULT_BUILD_TYPE} CACHE STRING
            "Choose the type of build." FORCE)

    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
            "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Based on the target system's processor and the compiler being used,
# set build variables indicating which hardware features can be targeted
# by the compiler. Note we DO NOT check which hardware features are supported
# by this (the host) system, because we want to be able to support compiling
# for newer hardware on older machines as well as cross-compilation.
message(STATUS "Building for system processor ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Building for compiler ID ${CMAKE_C_COMPILER_ID}")
if(CMAKE_SYSTEM_PROCESSOR STREQUAL i386 OR
        CMAKE_SYSTEM_PROCESSOR STREQUAL i686 OR
        CMAKE_SYSTEM_PROCESSOR STREQUAL x86_64 OR
        CMAKE_SYSTEM_PROCESSOR STREQUAL amd64 OR
        CMAKE_SYSTEM_PROCESSOR STREQUAL AMD64)
    if(CMAKE_C_COMPILER_ID STREQUAL GNU)
        # We need C99 (GNU99 more exactly)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
        set(COMPILER_SUPPORT_SSE2 TRUE)
        if(CMAKE_C_COMPILER_VERSION VERSION_GREATER 4.7 OR CMAKE_C_COMPILER_VERSION VERSION_EQUAL 4.7)
            set(COMPILER_SUPPORT_AVX2 TRUE)
        else()
            set(COMPILER_SUPPORT_AVX2 FALSE)
        endif()
    elseif(CMAKE_C_COMPILER_ID STREQUAL Clang OR CMAKE_C_COMPILER_ID STREQUAL AppleClang)
        set(COMPILER_SUPPORT_SSE2 TRUE)
        if(CMAKE_C_COMPILER_VERSION VERSION_GREATER 3.2 OR CMAKE_C_COMPILER_VERSION VERSION_EQUAL 3.2)
            set(COMPILER_SUPPORT_AVX2 TRUE)
        else()
            set(COMPILER_SUPPORT_AVX2 FALSE)
        endif()
    elseif(CMAKE_C_COMPILER_ID STREQUAL Intel)
        set(COMPILER_SUPPORT_SSE2 TRUE)
        if(CMAKE_C_COMPILER_VERSION VERSION_GREATER 14.0 OR CMAKE_C_COMPILER_VERSION VERSION_EQUAL 14.0)
            set(COMPILER_SUPPORT_AVX2 TRUE)
        else()
            set(COMPILER_SUPPORT_AVX2 FALSE)
        endif()
    elseif(MSVC)
        set(COMPILER_SUPPORT_SSE2 TRUE)
        if(CMAKE_C_COMPILER_VERSION VERSION_GREATER 18.00.30501 OR CMAKE_C_COMPILER_VERSION VERSION_EQUAL 18.00.30501)
            set(COMPILER_SUPPORT_AVX2 TRUE)
        else()
            set(COMPILER_SUPPORT_AVX2 FALSE)
        endif()
    else()
        set(COMPILER_SUPPORT_SSE2 FALSE)
        set(COMPILER_SUPPORT_AVX2 FALSE)
        # Unrecognized compiler. Emit a warning message to let the user know hardware-acceleration won't be available.
        message(WARNING "Unable to determine which ${CMAKE_SYSTEM_PROCESSOR} hardware features are supported by the C compiler (${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}).")
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL armv7l OR
        CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64 OR
        CMAKE_SYSTEM_PROCESSOR STREQUAL arm64)
    if(CMAKE_C_COMPILER_ID STREQUAL GNU)
        if(CMAKE_C_COMPILER_VERSION VERSION_GREATER 5.2 OR CMAKE_C_COMPILER_VERSION VERSION_EQUAL 5.2)
            set(COMPILER_SUPPORT_NEON TRUE)
        else()
            set(COMPILER_SUPPORT_NEON FALSE)
        endif()
    elseif(CMAKE_C_COMPILER_ID STREQUAL Clang OR CMAKE_C_COMPILER_ID STREQUAL AppleClang)
        if(CMAKE_C_COMPILER_VERSION VERSION_GREATER 3.3 OR CMAKE_C_COMPILER_VERSION VERSION_EQUAL 3.3)
            set(COMPILER_SUPPORT_NEON TRUE)
        else()
            set(COMPILER_SUPPORT_NEON FALSE)
        endif()
    else()
        set(COMPILER_SUPPORT_NEON FALSE)
        # Unrecognized compiler. Emit a warning message to let the user know hardware-acceleration won't be available.
        message(WARNING "Unable to determine which ${CMAKE_SYSTEM_PROCESSOR} hardware features are supported by the C compiler (${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}).")
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL ppc64le)
    if(CMAKE_C_COMPILER_ID STREQUAL GNU AND CMAKE_C_COMPILER_VERSION VERSION_GREATER 8)
        set(COMPILER_SUPPORT_ALTIVEC TRUE)
    else()
        set(COMPILER_SUPPORT_ALTIVEC FALSE)
    endif()
else()
    # If the target system processor isn't recognized, emit a warning message to alert the user
    # that hardware-acceleration support won't be available but allow configuration to proceed.
    message(WARNING "Unrecognized system processor ${CMAKE_SYSTEM_PROCESSOR}. Cannot determine which hardware features (${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}) supports, so hardware-accelerated implementations will not be available.")
endif()

# disable AVX2 if specified
if(DEACTIVATE_AVX2)
    set(COMPILER_SUPPORT_AVX2 FALSE)
endif()

# flags
# @TODO: set -Wall
# @NOTE: -O3 is enabled in Release mode (CMAKE_BUILD_TYPE="Release")

# Set the "-msse2" build flag only if the CMAKE_C_FLAGS is not already set.
# Probably "-msse2" should be appended to CMAKE_C_FLAGS_RELEASE.
if(CMAKE_C_COMPILER_ID STREQUAL GNU OR CMAKE_C_COMPILER_ID STREQUAL Clang OR CMAKE_C_COMPILER_ID STREQUAL Intel)
    if(NOT CMAKE_C_FLAGS AND COMPILER_SUPPORT_SSE2)
        set(CMAKE_C_FLAGS -msse2 CACHE STRING "C flags." FORCE)
    endif()
endif()

if(CMAKE_C_COMPILER_ID STREQUAL Intel OR CMAKE_C_COMPILER_ID STREQUAL Clang OR HAIKU)
    # We need to tell Intel and Clang compilers about what level of POSIX they support
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_XOPEN_SOURCE=600")
endif()

if(MSVC)
    if(NOT CMAKE_C_FLAGS)
        set(CMAKE_C_FLAGS "/Ox" CACHE STRING "C flags." FORCE)
    endif()

    # Turn off misguided "secure CRT" warnings in MSVC.
    # Microsoft wants people to use the MS-specific <function>_s
    # versions of certain C functions but this is difficult to do
    # in platform-independent code.
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

if(WIN32)
    # For some supporting headers
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/blosc")
endif()

if(NOT DEFINED BLOSC_IS_SUBPROJECT)
    if("^${CMAKE_SOURCE_DIR}$" STREQUAL "^${PROJECT_SOURCE_DIR}$")
        set (BLOSC_IS_SUBPROJECT FALSE)
    else()
        set (BLOSC_IS_SUBPROJECT TRUE)
        message(STATUS "Detected that BLOSC is used a subproject.")
   endif()
endif()

if(NOT DEFINED BLOSC_INSTALL)
    if(BLOSC_IS_SUBPROJECT)
        set(BLOSC_INSTALL FALSE)
    else()
        set(BLOSC_INSTALL TRUE)
    endif()
endif()


# include directories
include_directories(include)

# subdirectories
add_subdirectory(blosc)


if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
    add_subdirectory(compat)
endif()

if(BUILD_FUZZERS)
    if(NOT BUILD_STATIC)
        message(FATAL_ERROR "BUILD_FUZZERS requires BUILD_STATIC to be enabled.")
    endif()
    enable_testing()
    add_subdirectory(tests/fuzz)
endif()

if(BUILD_BENCHMARKS)
    add_subdirectory(bench)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_PLUGINS)
    enable_testing()
    add_subdirectory(plugins)
endif()


# uninstall target
if(BLOSC_INSTALL)
    configure_file(
         "${CMAKE_CURRENT_SOURCE_DIR}/blosc.pc.in"
         "${CMAKE_CURRENT_BINARY_DIR}/blosc.pc"
         @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/blosc.pc"
            DESTINATION lib/pkgconfig COMPONENT DEV)
     configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY)
    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()

# packaging
if(NOT BLOSC_IS_SUBPROJECT)
    include(InstallRequiredSystemLibraries)

    set(CPACK_GENERATOR TGZ ZIP)
    set(CPACK_SOURCE_GENERATOR TGZ ZIP)
    set(CPACK_PACKAGE_VERSION_MAJOR ${BLOSC_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${BLOSC_VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${BLOSC_VERSION_PATCH})
    set(CPACK_PACKAGE_VERSION ${BLOSC_STRING_VERSION})
    set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.rst")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
            "A blocking, shuffling and lossless compression library")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
    set(CPACK_SOURCE_IGNORE_FILES "/build.*;.*~;\\\\.git.*;\\\\.DS_Store")
    set(CPACK_STRIP_FILES TRUE)
    set(CPACK_SOURCE_STRIP_FILES TRUE)

    include(CPack)
endif()
