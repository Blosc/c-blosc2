# Blosc - Blocked Shuffling and Compression Library
#
# Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
# https://blosc.org
# License: BSD 3-Clause (see LICENSE.txt)
#
# See LICENSE.txt for details about copyright and rights to use.

# sources
get_filename_component(NDLZ_LOCAL_DIR ./ ABSOLUTE)
file(GLOB NDLZ_SRC_FILES ${NDLZ_LOCAL_DIR}/ndlz.* ${NDLZ_LOCAL_DIR}/ndlz*.c ${NDLZ_LOCAL_DIR}/ndlz*.h
        ${NDLZ_LOCAL_DIR}/xxhash.*)
#message(status "--------------!!!!zfp:" ${ZFP_SRC_FILES})
set(SOURCES ${SOURCES} ${NDLZ_SRC_FILES} PARENT_SCOPE)

# targets
if(BUILD_TESTS)
    add_executable(test_ndlz test_ndlz.c)
    # Define the BLOSC_TESTING symbol so normally-hidden functions
    # aren't hidden from the view of the test programs.
    set_property(
            TARGET test_ndlz
            APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_TESTING)

    target_link_libraries(test_ndlz blosc_testing)

    # tests
        add_test(NAME test_plugin_test_ndlz
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:test_ndlz>)

    # Copy test files
    file(GLOB TESTS_DATA ../../test_data/example_s*.b2nd)
    foreach (data ${TESTS_DATA})
        file(COPY ${data}
                DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
    endforeach(data)
endif()
