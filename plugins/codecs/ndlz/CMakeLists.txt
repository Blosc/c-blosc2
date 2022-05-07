# sources
set(SOURCES ${SOURCES} ../plugins/codecs/ndlz/ndlz.c ../plugins/codecs/ndlz/ndlz.h ../plugins/codecs/ndlz/ndlz-private.h
    ../plugins/codecs/ndlz/ndlz4x4.c ../plugins/codecs/ndlz/ndlz4x4.h ../plugins/codecs/ndlz/ndlz8x8.c
    ../plugins/codecs/ndlz/ndlz8x8.h ../plugins/codecs/ndlz/xxhash.c ../plugins/codecs/ndlz/xxhash.h PARENT_SCOPE)

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
    file(GLOB TESTS_DATA ../../test_data/example_s*.caterva)
    foreach (data ${TESTS_DATA})
        file(COPY ${data}
                DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
    endforeach(data)
endif()
