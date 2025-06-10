# Blosc - Blocked Shuffling and Compression Library
#
# Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
# https://blosc.org
# License: BSD 3-Clause (see LICENSE.txt)
#
# See LICENSE.txt for details about copyright and rights to use.

# sources
set(SOURCES ${SOURCES}
        ${PROJECT_SOURCE_DIR}/plugins/codecs/ndlz/ndlz.c
        ${PROJECT_SOURCE_DIR}/plugins/codecs/ndlz/ndlz4x4.c
        ${PROJECT_SOURCE_DIR}/plugins/codecs/ndlz/ndlz8x8.c
        ${PROJECT_SOURCE_DIR}/plugins/codecs/ndlz/xxhash.c
        PARENT_SCOPE)

# targets
if(BUILD_TESTS)
    add_executable(test_ndlz test_ndlz.c)
    # Define the BLOSC_TESTING symbol so normally-hidden functions
    # aren't hidden from the view of the test programs.
    target_compile_definitions(test_ndlz PUBLIC BLOSC_TESTING)

    target_link_libraries(test_ndlz PUBLIC blosc_testing)

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
