get_filename_component(ZFP_LOCAL_DIR ./ ABSOLUTE)
file(GLOB ZFP_SRC_FILES ${ZFP_LOCAL_DIR}/blosc2-zfp.* ${ZFP_LOCAL_DIR}/src/zfp.c
     ${ZFP_LOCAL_DIR}/src/bitstream.c ${ZFP_LOCAL_DIR}/src/decode*.c ${ZFP_LOCAL_DIR}/src/encode*.c)
#message(status "--------------!!!!zfp:" ${ZFP_SRC_FILES})
set(SOURCES ${SOURCES} ${ZFP_SRC_FILES} PARENT_SCOPE)

# targets
if(BUILD_TESTS)
    add_executable(test_zfp_acc_float test_zfp_acc_float.c)
    add_executable(test_zfp_prec_float test_zfp_prec_float.c)
    add_executable(test_zfp_rate_float test_zfp_rate_float.c)
    add_executable(test_zfp_rate_getitem test_zfp_rate_getitem.c)
    # Define the BLOSC_TESTING symbol so normally-hidden functions
    # aren't hidden from the view of the test programs.
    set_property(
            TARGET test_zfp_acc_float
            APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_TESTING)
    set_property(
            TARGET test_zfp_prec_float
            APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_TESTING)
    set_property(
            TARGET test_zfp_rate_float
            APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_TESTING)
    set_property(
            TARGET test_zfp_rate_getitem
            APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_TESTING)

    target_link_libraries(test_zfp_acc_float blosc_testing)
    target_link_libraries(test_zfp_prec_float blosc_testing)
    target_link_libraries(test_zfp_rate_float blosc_testing)
    target_link_libraries(test_zfp_rate_getitem blosc_testing)

    # tests
    add_test(NAME test_plugin_test_zfp_acc_float
        COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:test_zfp_acc_float>)
    add_test(NAME test_plugin_test_zfp_prec_float
        COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:test_zfp_prec_float>)
    add_test(NAME test_plugin_test_zfp_rate_float
        COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:test_zfp_rate_float>)
    add_test(NAME test_plugin_test_zfp_rate_getitem
        COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:test_zfp_rate_getitem>)

    # Copy test files
    file(GLOB TESTS_DATA ../../test_data/example_day_month_temp.b2nd ../../test_data/example_item_prices.b2nd)
    foreach (data ${TESTS_DATA})
        file(COPY ${data}
                DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
    endforeach(data)
endif()
