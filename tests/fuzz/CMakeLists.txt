# flags
link_directories(${PROJECT_BINARY_DIR}/blosc)

# look for fuzzing lib and link with it if found
if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    enable_language(CXX)

    if(DEFINED ENV{LIB_FUZZING_ENGINE})
        set(FUZZING_ENGINE $ENV{LIB_FUZZING_ENGINE})
        set(FUZZING_ENGINE_FOUND TRUE)
    else()
        find_library(FUZZING_ENGINE "FuzzingEngine")
    endif()
endif()

# If fuzzing lib not found then create standalone fuzz runner
if(NOT FUZZING_ENGINE_FOUND)
    set(FUZZER_SRC standalone.c)
else()
    set(FUZZER_SRC)
endif()

# sources
file(GLOB SOURCES fuzz_*.c)

# targets and tests
foreach(source ${SOURCES})
    get_filename_component(target ${source} NAME_WE)

    # Enable support for testing accelerated shuffles
    if(COMPILER_SUPPORT_SSE2 AND SSE2_FOUND)
        # Define a symbol so tests for SSE2 shuffle/unshuffle will be compiled in.
        set_property(
                SOURCE ${source}
                APPEND PROPERTY COMPILE_DEFINITIONS SHUFFLE_SSE2_ENABLED)
    endif()

    if(COMPILER_SUPPORT_AVX2 AND AVX2_FOUND)
        # Define a symbol so tests for AVX2 shuffle/unshuffle will be compiled in.
        set_property(
                SOURCE ${source}
                APPEND PROPERTY COMPILE_DEFINITIONS SHUFFLE_AVX2_ENABLED)
    endif()

    if(COMPILER_SUPPORT_NEON)
        # Define a symbol so tests for NEON shuffle/unshuffle will be compiled in.
        set_property(
                SOURCE ${source}
                APPEND PROPERTY COMPILE_DEFINITIONS SHUFFLE_NEON_ENABLED)
    endif()

    if(COMPILER_SUPPORT_ALTIVEC)
        # Define a symbol so tests for NEON shuffle/unshuffle will be compiled in.
        set_property(
                SOURCE ${source}
                APPEND PROPERTY COMPILE_DEFINITIONS SHUFFLE_ALTIVEC_ENABLED)
    endif()

    add_executable(${target} ${source} ${FUZZER_SRC})

    # OSS-Fuzz expect fuzzers to end with _fuzzer
    string(REPLACE "fuzz_" "" output_name ${target})
    set_target_properties(${target} PROPERTIES OUTPUT_NAME ${output_name}_fuzzer)

    if(FUZZING_ENGINE_FOUND)
        set_target_properties(${target} PROPERTIES LINKER_LANGUAGE CXX)
        target_link_libraries(${target} ${FUZZING_ENGINE})
    endif()

    target_link_libraries(${target} blosc2_static)
    add_dependencies(${target} blosc2_static)

    # run standalone fuzzer against each file
    file(GLOB COMPAT_FILES ${PROJECT_SOURCE_DIR}/compat/*.cdata)
    add_test(NAME ${target} COMMAND ${target} ${COMPAT_FILES})

endforeach()
