# build the filegen utility
link_directories(${PROJECT_BINARY_DIR}/blosc)
add_executable(filegen filegen.c)
target_link_libraries(filegen blosc_testing)
add_dependencies(filegen blosc_testing)

# tests
if(BUILD_TESTS)
    option(TEST_INCLUDE_COMPAT "Include compat checks in the tests" ON)
    if(TEST_INCLUDE_COMPAT)
        file(GLOB DATAFILES *.cdata)
        foreach(datafile ${DATAFILES})
            # Don't test data if compressor is deactivated
            if((datafile MATCHES "zlib" AND DEACTIVATE_ZLIB) OR
               (datafile MATCHES "zstd" AND DEACTIVATE_ZSTD))
               continue()
            endif()
            get_filename_component(fname ${datafile} NAME)
            add_test(test_compat_${fname} filegen decompress ${datafile})
        endforeach()
    endif()
endif()
