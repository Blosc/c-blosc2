get_filename_component(ZFP_LOCAL_DIR ./ ABSOLUTE)
file(GLOB ZFP_SRC_FILES ${ZFP_LOCAL_DIR}/blosc2-zfp.* ${ZFP_LOCAL_DIR}/zfp-private.h ${ZFP_LOCAL_DIR}/src/zfp.c
     ${ZFP_LOCAL_DIR}/src/bitstream.c ${ZFP_LOCAL_DIR}/src/decode*.c ${ZFP_LOCAL_DIR}/src/encode*.c)
#message(status "--------------!!!!zfp:" ${ZFP_SRC_FILES})
set(SOURCES ${SOURCES} ${ZFP_SRC_FILES} PARENT_SCOPE)

# targets
if(BUILD_TESTS)
    add_executable(test_zfp_int test_zfp_int.c)
    add_executable(test_zfp_float test_zfp_float.c)
    # Define the BLOSC_TESTING symbol so normally-hidden functions
    # aren't hidden from the view of the test programs.
    set_property(
            TARGET test_zfp_int
            APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_TESTING)
    set_property(
            TARGET test_zfp_float
            APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_TESTING)

    target_link_libraries(test_zfp_int blosc_testing)
    target_link_libraries(test_zfp_float blosc_testing)

    # tests
    add_test(test_plugin_test_zfp_int test_zfp_int)
    add_test(test_plugin_test_zfp_float test_zfp_float)

    # Copy test files
    file(GLOB TESTS_DATA ../../test_data/example_float_cyclic.caterva ../../test_data/example_double_same_cells.caterva
            ../../test_data/example_big_float_frame.caterva ../../test_data/example_day_month_temp.caterva
            ../../test_data/example_item_prices.caterva ../../filters/ndmean/example_ndmean_r*.caterva)
    foreach (data ${TESTS_DATA})
        file(COPY ${data}
                DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
    endforeach(data)
endif()
