add_subdirectory(src)

set(SOURCES ${SOURCES} blosc2-zfp.c blosc2-zfp.h zfp-private.h)

# targets
if(BUILD_TESTS)
    add_executable(test_zfp test_zfp.c ${SOURCES})
    # Define the BLOSC_TESTING symbol so normally-hidden functions
    # aren't hidden from the view of the test programs.
    set_property(
            TARGET test_zfp
            APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_TESTING)

    target_link_libraries(test_zfp blosc_testing)

    # tests
    add_test(test_plugin_test_zfp test_zfp)

    # Copy test files
    file(GLOB TESTS_DATA ../../filters/ndmean/example_ndmean_r*.caterva)
    foreach (data ${TESTS_DATA})
        file(COPY ${data}
                DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
    endforeach(data)
endif()
