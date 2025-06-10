# Examples with correspondingly named source files
set(EXAMPLES contexts instrument_codec delta_schunk_ex multithread simple frame_metalayers
    noinit find_roots schunk_simple frame_simple schunk_postfilter urcodecs urfilters frame_vlmetalayers
    sframe_simple frame_backed_schunk compress_file decompress_file frame_offset frame_roundtrip get_set_slice get_blocksize)

add_subdirectory(b2nd)

if(NOT DEACTIVATE_ZSTD)
    set(EXAMPLES ${EXAMPLES} zstd_dict)
endif()

if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
    # frame_big uses containers of more than 4 GB and cannot run on 32-bit platforms
    list(APPEND EXAMPLES frame_big)
endif()

# targets
foreach(example ${EXAMPLES})
    add_executable(${example} ${example}.c)
    # Define the BLOSC_TESTING symbol so normally-hidden functions
    # aren't hidden from the view of the test programs.
    set_property(
            TARGET ${example}
            APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_TESTING)
endforeach()

if(UNIX AND NOT APPLE)
    # cmake is complaining about LINK_PRIVATE in original PR
    # and removing it does not seem to hurt, so be it.
    # target_link_libraries(bench LINK_PRIVATE rt)
    foreach(example ${EXAMPLES})
        target_link_libraries(${example} rt)
    endforeach()
endif()

foreach(example ${EXAMPLES})
    target_link_libraries(${example} blosc_testing)
endforeach()

# tests
if(BUILD_TESTS)
    option(TEST_INCLUDE_EXAMPLES "Include examples in the tests" ON)
    if(TEST_INCLUDE_EXAMPLES)
        foreach(example ${EXAMPLES})
            if(example STREQUAL compress_file)
                add_test(NAME test_example_${example}
                         COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:${example}>
                         "${PROJECT_BINARY_DIR}/CMakeCache.txt" CMakeCache.b2frame)
            elseif(example STREQUAL decompress_file)
                add_test(NAME test_example_${example}
                         COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:${example}>
                         CMakeCache.b2frame CMakeCache-2.txt)
            else()
                add_test(NAME test_example_${example}
                    COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:${example}>)
            endif()
        endforeach()
    endif()
endif()
