# sources
set(SOURCES ${SOURCES} ${PROJECT_SOURCE_DIR}/plugins/filters/ndcell/ndcell.c PARENT_SCOPE)

if(BUILD_TESTS)

    # targets
    add_executable(test_ndcell test_ndcell.c)
    # Define the BLOSC_TESTING symbol so normally-hidden functions
    # aren't hidden from the view of the test programs.
    set_property(
            TARGET test_ndcell
            APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_TESTING)

    target_link_libraries(test_ndcell blosc_testing)

    # tests
    add_test(NAME test_plugin_test_ndcell
        COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:test_ndcell>)

    # Copy test files
    file(GLOB TESTS_DATA ../../test_data/*.b2nd)
    foreach (data ${TESTS_DATA})
        file(COPY ${data}
                DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
    endforeach(data)
endif()
