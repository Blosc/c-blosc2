# sources
set(SOURCES ndcell.c ndcell.h)

# targets
add_executable(test_ndcell test_ndcell.c ${SOURCES})
# Define the BLOSC_TESTING symbol so normally-hidden functions
# aren't hidden from the view of the test programs.
set_property(
        TARGET test_ndcell
        APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_TESTING)

target_link_libraries(test_ndcell blosc_testing)

# tests
if(BUILD_TESTS)
    add_test(test_plugin_test_ndcell test_ndcell)
endif()

# Copy test files
file(GLOB TESTS_DATA ../../test_data/*.caterva)
foreach (data ${TESTS_DATA})
    file(COPY ${data}
            DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
endforeach(data)
