# sources
set(SOURCES ${SOURCES} ../plugins/filters/ndmean/ndmean.c ../plugins/filters/ndmean/ndmean.h PARENT_SCOPE)

if(BUILD_TESTS)
    # targets
    add_executable(test_ndmean_repart test_ndmean_repart.c)
    add_executable(test_ndmean_mean test_ndmean_mean.c)
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
    add_test(NAME test_plugin_ndmean_repart
        COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:test_ndmean_repart>)
    add_test(NAME test_plugin_ndmean_mean
        COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:test_ndmean_mean>)

    # Copy test files
    file(GLOB TESTS_DATA *.caterva)
    foreach (data ${TESTS_DATA})
        file(COPY ${data}
                DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
    endforeach(data)
endif()
