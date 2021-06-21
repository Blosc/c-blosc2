# sources for examples
set(SOURCES contexts delta_schunk_ex multithread simple frame_metalayers noinit find_roots)
set(SOURCES ${SOURCES} schunk_simple frame_simple urcodecs urfilters frame_vlmetalayers sframe_simple frame_backed_schunk compress_file)
if(NOT DEACTIVATE_ZSTD)
    set(SOURCES ${SOURCES} zstd_dict)
endif()

if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
    # frame_big uses containers of more than 4 GB and cannot run on 32-bit platforms
    set(SOURCES ${SOURCES} frame_big)
endif()

if(EXISTS btune_schunk.c)
    set(SOURCES ${SOURCES} btune_schunk.c)
endif()

# targets
foreach(src ${SOURCES})
    add_executable(${src} ${src}.c)
    # Define the BLOSC_TESTING symbol so normally-hidden functions
    # aren't hidden from the view of the test programs.
    set_property(
            TARGET ${src}
            APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_TESTING)
endforeach()

if(UNIX AND NOT APPLE)
    # cmake is complaining about LINK_PRIVATE in original PR
    # and removing it does not seem to hurt, so be it.
    # target_link_libraries(bench LINK_PRIVATE rt)
    foreach(src ${SOURCES})
        target_link_libraries(${src} rt)
    endforeach()
endif()

foreach(src ${SOURCES})
    target_link_libraries(${src} blosc_testing)
endforeach()

# tests
if(BUILD_TESTS)
    option(TEST_INCLUDE_EXAMPLES "Include examples in the tests" ON)
    if(TEST_INCLUDE_EXAMPLES)
        foreach(src ${SOURCES})
            if(src STREQUAL compress_file)
                add_test(test_example_${src} ${src} "${PROJECT_BINARY_DIR}/CMakeCache.txt" CMakeCache.b2frame)
            else()
                add_test(test_example_${src} ${src})
            endif()
        endforeach()
    endif()
endif()
