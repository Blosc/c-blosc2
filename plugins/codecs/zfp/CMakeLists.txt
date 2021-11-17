
set(ZFP_LOCAL_DIR ../plugins/codecs/zfp/src)
#file(GLOB ZFP_SRC_FILES ${ZFP_LOCAL_DIR}/decode*.c ${ZFP_LOCAL_DIR}/encode*.c)
#message(status "--------------!!!!zfp:" ${ZFP_SRC_FILES})

set(SOURCES ${SOURCES} ../plugins/codecs/zfp/blosc2-zfp.c ../plugins/codecs/zfp/blosc2-zfp.h ../plugins/codecs/zfp/zfp-private.h
    ${ZFP_LOCAL_DIR}/decode1d.c ${ZFP_LOCAL_DIR}/decode1f.c ${ZFP_LOCAL_DIR}/decode1i.c ${ZFP_LOCAL_DIR}/decode1l.c
    ${ZFP_LOCAL_DIR}/decode2d.c ${ZFP_LOCAL_DIR}/decode2f.c ${ZFP_LOCAL_DIR}/decode2i.c ${ZFP_LOCAL_DIR}/decode2l.c
    ${ZFP_LOCAL_DIR}/decode3d.c ${ZFP_LOCAL_DIR}/decode3f.c ${ZFP_LOCAL_DIR}/decode3i.c ${ZFP_LOCAL_DIR}/decode3l.c
    ${ZFP_LOCAL_DIR}/decode4d.c ${ZFP_LOCAL_DIR}/decode4f.c ${ZFP_LOCAL_DIR}/decode4i.c ${ZFP_LOCAL_DIR}/decode4l.c
    ${ZFP_LOCAL_DIR}/encode1d.c ${ZFP_LOCAL_DIR}/encode1f.c ${ZFP_LOCAL_DIR}/encode1i.c ${ZFP_LOCAL_DIR}/encode1l.c
    ${ZFP_LOCAL_DIR}/encode2d.c ${ZFP_LOCAL_DIR}/encode2f.c ${ZFP_LOCAL_DIR}/encode2i.c ${ZFP_LOCAL_DIR}/encode2l.c
    ${ZFP_LOCAL_DIR}/encode3d.c ${ZFP_LOCAL_DIR}/encode3f.c ${ZFP_LOCAL_DIR}/encode3i.c ${ZFP_LOCAL_DIR}/encode3l.c
    ${ZFP_LOCAL_DIR}/encode4d.c ${ZFP_LOCAL_DIR}/encode4f.c ${ZFP_LOCAL_DIR}/encode4i.c ${ZFP_LOCAL_DIR}/encode4l.c
        ${ZFP_LOCAL_DIR}/zfp.c ${ZFP_LOCAL_DIR}/bitstream.c
    PARENT_SCOPE)

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
