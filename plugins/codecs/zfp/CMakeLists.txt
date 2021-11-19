get_filename_component(ZFP_LOCAL_DIR ./ ABSOLUTE)
file(GLOB ZFP_SRC_FILES ${ZFP_LOCAL_DIR}/blosc2-zfp.* ${ZFP_LOCAL_DIR}/zfp-private.h ${ZFP_LOCAL_DIR}/src/zfp.c
     ${ZFP_LOCAL_DIR}/src/bitstream.c ${ZFP_LOCAL_DIR}/src/decode*.c ${ZFP_LOCAL_DIR}/src/encode*.c)
#message(status "--------------!!!!zfp:" ${ZFP_SRC_FILES})
set(SOURCES ${SOURCES} ${ZFP_SRC_FILES} PARENT_SCOPE)

# targets
if(BUILD_TESTS)
    add_executable(test_zfp test_zfp.c)
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
