# sources for examples
#set(SOURCES contexts delta_schunk_ex multithread simple noinit simple_schunk)
set(SOURCES contexts delta_schunk_ex multithread simple noinit find_roots)

# targets
foreach(src ${SOURCES})
    add_executable(${src} ${src}.c)
endforeach(src)

if (UNIX AND NOT APPLE)
    # cmake is complaining about LINK_PRIVATE in original PR
    # and removing it does not seem to hurt, so be it.
    # target_link_libraries(bench LINK_PRIVATE rt)
    foreach(src ${SOURCES})
        target_link_libraries(${src} rt)
    endforeach(src)
endif (UNIX AND NOT APPLE)

foreach(src ${SOURCES})
    target_link_libraries(${src} blosc_shared)
endforeach(src)

# have to copy blosc dlls on Windows
if (MSVC)
    add_custom_command(
            TARGET contexts
            POST_BUILD
            COMMAND ${CMAKE_COMMAND}
            ARGS -E copy_if_different
            "${PROJECT_BINARY_DIR}/blosc/\$\(Configuration\)/blosc.dll"
            "${CMAKE_CURRENT_BINARY_DIR}/\$\(Configuration\)/blosc.dll")
elseif (MINGW)
    add_custom_command(
            TARGET contexts
            POST_BUILD
            COMMAND ${CMAKE_COMMAND}
            ARGS -E copy_if_different
            "${PROJECT_BINARY_DIR}/blosc/libblosc.dll"
            "${CMAKE_CURRENT_BINARY_DIR}/libblosc.dll")
endif (MSVC)

# tests
if (BUILD_TESTS)
    option(TEST_INCLUDE_EXAMPLES "Include examples in the tests" ON)
    if (TEST_INCLUDE_EXAMPLES)
        foreach(src ${SOURCES})
            add_test(test_example_${src} ${src})
        endforeach(src)
    endif()
endif (BUILD_TESTS)
