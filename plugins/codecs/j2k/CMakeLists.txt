add_subdirectory(OpenHTJ2K)
add_library(blosc2_htj2k STATIC blosc2_htj2k.cpp)
add_library(blosc2_htj2k_shared SHARED blosc2_htj2k.cpp)
if(MSVC OR MINGW)
    # Necessary for compiling blosc2_htj2k_shared.lib
    set_property(TARGET blosc2_htj2k_shared PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()
target_link_libraries(blosc2_htj2k_shared open_htj2k)
target_link_libraries(blosc2_htj2k open_htj2k)

# targets
if(BUILD_TESTS)
    add_executable(test_j2k test_j2k.c)
    # Define the BLOSC_TESTING symbol so normally-hidden functions
    # aren't hidden from the view of the test programs.
    set_property(
            TARGET test_j2k
            APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_TESTING)

    target_link_libraries(test_j2k blosc_testing)

    # tests
    add_test(NAME test_plugin_test_j2k
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:test_j2k>)

    # Copy test files
    file(GLOB TESTS_DATA ../../test_data/teapot.ppm)
    foreach (data ${TESTS_DATA})
        file(COPY ${data}
                DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
    endforeach(data)
endif()
