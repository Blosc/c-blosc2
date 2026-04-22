set(SOURCES ${SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/blosc2-zfp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/blosc2-zfp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zfp-private.h
    PARENT_SCOPE)

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
