# sources
file(GLOB SOURCES test_*.c fuzz/generate_inputs_corpus.c)

# flags
link_directories(${PROJECT_BINARY_DIR}/blosc)
include_directories(${PROJECT_SOURCE_DIR}/blosc)

# Check SIMD features for the local CPU
if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL arm64)
    find_package(SIMD)
endif()

# targets and tests
foreach(source ${SOURCES})
    get_filename_component(target ${source} NAME_WE)

    # Some tests will be enabled only for Unix
    if(WIN32)
        if(target STREQUAL test_nolock OR
            target STREQUAL test_noinit OR
            target STREQUAL test_compressor OR
            target STREQUAL test_blosc1_compat)
            message("Skipping ${target} on Windows systems")
            continue()
        endif()
    endif()

    # Disable targets that use zstd compressor when zstd is deactivated
    if((target STREQUAL test_dict_schunk) AND DEACTIVATE_ZSTD)
        message("Skipping ${target} on non-ZSTD builds")
        continue()
    endif()

    # Enable support for testing accelerated shuffles
    if(COMPILER_SUPPORT_SSE2 AND SSE2_FOUND)
        # Define a symbol so tests for SSE2 shuffle/unshuffle will be compiled in *and* there is support in the CPU for it.
        set_property(
                SOURCE ${source}
                APPEND PROPERTY COMPILE_DEFINITIONS SHUFFLE_SSE2_ENABLED)
    elseif(target STREQUAL test_shuffle_roundtrip_sse2)
        message("Skipping ${target} on non-SSE2 builds")
        continue()
    endif()

    if(COMPILER_SUPPORT_AVX2 AND AVX2_FOUND)
        # Define a symbol so tests for AVX2 shuffle/unshuffle will be compiled in *and* there is support in the CPU for it.
        set_property(
                SOURCE ${source}
                APPEND PROPERTY COMPILE_DEFINITIONS SHUFFLE_AVX2_ENABLED)
    elseif(target STREQUAL test_shuffle_roundtrip_avx2)
        message("Skipping ${target} on non-AVX2 builds")
        continue()
    endif()

    if(COMPILER_SUPPORT_NEON)
         # Define a symbol so tests for NEON shuffle/unshuffle will be compiled in.
         set_property(
                SOURCE ${source}
                APPEND PROPERTY COMPILE_DEFINITIONS SHUFFLE_NEON_ENABLED)
    elseif(target STREQUAL test_shuffle_roundtrip_neon)
        message("Skipping ${target} on non-NEON builds")
        continue()
    endif()

    if(COMPILER_SUPPORT_ALTIVEC)
         # Define a symbol so tests for NEON shuffle/unshuffle will be compiled in.
         set_property(
                SOURCE ${source}
                APPEND PROPERTY COMPILE_DEFINITIONS SHUFFLE_ALTIVEC_ENABLED)
    elseif(target STREQUAL test_shuffle_roundtrip_altivec)
        message("Skipping ${target} on non-ALTIVEC builds")
        continue()
    endif()

    add_executable(${target} ${source})

    # Define the BLOSC_TESTING symbol so normally-hidden functions
    # aren't hidden from the view of the test programs.
    set_property(
            TARGET ${target}
            APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_TESTING)

    target_link_libraries(${target} blosc_testing)
    add_dependencies(${target} blosc_testing)

    # If there's a CSV file present for this test, read it to get the list
    # of test parameters then add a test for each parameter set.
    # Otherwise, this is a simple test so just add it once.
    get_filename_component(source_extension ${source} EXT)
    string(REGEX REPLACE "${source_extension}$" ".csv"
            test_params_file ${source})
    if(EXISTS "${test_params_file}")
        # Read the file contents into a CMake list
        file(READ "${test_params_file}" test_params_contents)

        string(REGEX REPLACE ";" "\\\\;"
                test_params_contents "${test_params_contents}")
        string(REGEX REPLACE "\n" ";"
                test_params_contents "${test_params_contents}")

        # How many parameter sets for this test?
        # If there's not at least one (accounting for the CSV header line),
        # that's probably not correct so emit an error and stop configuring.
        list(LENGTH test_params_contents test_params_count)
        if("${test_params_count}" LESS 2)
            message(ERROR "Invalid test parameters file: ${test_params_file}")
        endif()

        # Remove the header line.
        list(REMOVE_AT test_params_contents 0)

        # Add a test for each parameter set in the file.
        foreach(test_params_raw ${test_params_contents})
            string(REGEX REPLACE "," " " test_params "${test_params_raw}")

            # Create the test name.
            # NOTE: The documentation for add_test says the test name "may not contain
            # spaces, quotes, or other characters special in CMake syntax."
            string(REGEX REPLACE "\"| " "_" test_name_params "${test_params}")
            set(test_name "${target}_${test_name_params}")

            separate_arguments(test_params)
            add_test(${test_name} ${target} ${test_params})
        endforeach()
    else()
        add_test(${target} ${target})
    endif()
endforeach()
