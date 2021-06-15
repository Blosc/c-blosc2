# sources
set(SOURCES ${SOURCES} ndlz.c ndlz.h ndlz-private.h ndlz4x4.c ndlz4x4.h ndlz8x8.c ndlz8x8.h xxhash.c xxhash.h)

# targets
if(BUILD_TESTS)
    add_executable(test_ndlz test_ndlz.c ${SOURCES})
    # Define the BLOSC_TESTING symbol so normally-hidden functions
    # aren't hidden from the view of the test programs.
    set_property(
            TARGET test_ndlz
            APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_TESTING)

    target_link_libraries(test_ndlz blosc_testing)

    # tests
        add_test(test_plugin_test_ndlz test_ndlz)

    # Copy test files
    file(GLOB TESTS_DATA ../../test_data/example_s*.caterva)
    foreach (data ${TESTS_DATA})
        file(COPY ${data}
                DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
    endforeach(data)
endif()
