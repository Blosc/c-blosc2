# sources
set(SOURCES ndmean.c ndmean.h)

# targets
add_executable(test_ndmean test_ndmean.c ${SOURCES})
# Define the BLOSC_TESTING symbol so normally-hidden functions
# aren't hidden from the view of the test programs.
set_property(
        TARGET test_ndmean
        APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_TESTING)

#if(UNIX AND NOT APPLE)
    # cmake is complaining about LINK_PRIVATE in original PR
    # and removing it does not seem to hurt, so be it.
    # target_link_libraries(bench LINK_PRIVATE rt)
 #   target_link_libraries(test_ndmean rt)
#endif()

target_link_libraries(test_ndmean blosc_testing)

# tests
if(BUILD_TESTS)
    add_test(test_plugin_test_ndmean test_ndmean)
endif()

# Copy test files
file(GLOB TESTS_DATA ../../test_data/*.caterva)
foreach (data ${TESTS_DATA})
    file(COPY ${data}
            DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
endforeach(data)
