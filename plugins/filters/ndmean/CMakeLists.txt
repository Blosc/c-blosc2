# sources
set(SOURCES ndmean.c ndmean.h)

if(BUILD_TESTS)
    # targets
    add_executable(test_ndmean_repart test_ndmean_repart.c ${SOURCES})
    add_executable(test_ndmean_mean test_ndmean_mean.c ${SOURCES})
    # Define the BLOSC_TESTING symbol so normally-hidden functions
    # aren't hidden from the view of the test programs.
    set_property(
            TARGET test_ndmean_mean
            APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_TESTING)
    set_property(
            TARGET test_ndmean_repart
            APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_TESTING)

    target_link_libraries(test_ndmean_repart blosc_testing)
    target_link_libraries(test_ndmean_mean blosc_testing)

    # tests
    add_test(test_plugin_ndmean_repart test_ndmean_repart)
    add_test(test_plugin_ndmean_mean test_ndmean_mean)

    # Copy test files
    file(GLOB TESTS_DATA *.caterva)
    foreach (data ${TESTS_DATA})
        file(COPY ${data}
                DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
    endforeach(data)
endif()
