# Blosc - Blocked Shuffling and Compression Library
#
# Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
# https://blosc.org
# License: BSD 3-Clause (see LICENSE.txt)
#
# See LICENSE.txt for details about copyright and rights to use.

# sources
set(SOURCES ${SOURCES} ${PROJECT_SOURCE_DIR}/plugins/filters/bytedelta/bytedelta.c PARENT_SCOPE)

if(BUILD_TESTS)
    add_executable(test_shuffle_bytedelta test_shuffle_bytedelta.c)
    target_link_libraries(test_shuffle_bytedelta blosc_testing)
    add_test(NAME test_shuffle_bytedelta COMMAND test_shuffle_bytedelta)
    add_executable(bench_shuffle_bytedelta bench_shuffle_bytedelta.c)
    target_link_libraries(bench_shuffle_bytedelta blosc_testing)
    # targets
    add_executable(test_bytedelta test_bytedelta.c)
    # Define the BLOSC_TESTING symbol so normally-hidden functions
    # are available to the test programs.
    set_property(
            TARGET test_bytedelta
            APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_TESTING)
    target_link_libraries(test_bytedelta blosc_testing)

    # tests
    add_test(NAME test_plugin_bytedelta
        COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:test_bytedelta>)

endif()
