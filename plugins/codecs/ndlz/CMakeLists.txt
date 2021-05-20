# sources
set(SOURCES ${SOURCES} ndlz.c ndlz.h ndlz4x4.c ndlz4x4.h ndlz8x8.c ndlz8x8.h)

# targets
add_executable(test_ndlz test_ndlz.c ${SOURCES})
# Define the BLOSC_TESTING symbol so normally-hidden functions
# aren't hidden from the view of the test programs.
set_property(
        TARGET test_ndlz
        APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_TESTING)

#if(UNIX AND NOT APPLE)
    # cmake is complaining about LINK_PRIVATE in original PR
    # and removing it does not seem to hurt, so be it.
    # target_link_libraries(bench LINK_PRIVATE rt)
 #   target_link_libraries(test_ndlz rt)
#endif()

target_link_libraries(test_ndlz blosc_testing)

# tests
if(BUILD_TESTS)
    add_test(test_plugin_test_ndlz test_ndlz)
endif()

# Copy test files
file(GLOB TESTS_DATA ../../test_data/example_s*.caterva)
foreach (data ${TESTS_DATA})
    file(COPY ${data}
            DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
endforeach(data)