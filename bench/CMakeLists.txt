# sources for main bench
set(SOURCES b2bench.c)
# other benchmarks
set(SOURCES_DELTA delta_schunk.c)
set(SOURCES_TRUNC_PREC trunc_prec_schunk.c)
set(SOURCES_SUM_OPENMP sum_openmp.c)
set(SOURCES_ZERO_RUNLEN zero_runlen.c)

# targets
set(BENCH_EXE b2bench)
add_executable(${BENCH_EXE} ${SOURCES})
add_executable(delta_schunk ${SOURCES_DELTA})
add_executable(trunc_prec_schunk ${SOURCES_TRUNC_PREC})
add_executable(sum_openmp ${SOURCES_SUM_OPENMP})
add_executable(zero_runlen ${SOURCES_ZERO_RUNLEN})
if(UNIX AND NOT APPLE)
    # cmake is complaining about LINK_PRIVATE in original PR
    # and removing it does not seem to hurt, so be it.
    # target_link_libraries(${BENCH_EXE} LINK_PRIVATE rt)
    target_link_libraries(${BENCH_EXE} rt)
    target_link_libraries(delta_schunk rt)
    target_link_libraries(trunc_prec_schunk rt)
    target_link_libraries(sum_openmp rt)
    target_link_libraries(zero_runlen rt)
endif()
if(UNIX)
    # Avoid a warning when using gcc without -fopenmp
    target_compile_options(sum_openmp PRIVATE "-Wno-unknown-pragmas")
endif()
target_link_libraries(${BENCH_EXE} blosc2_shared)
target_link_libraries(delta_schunk blosc2_shared)
target_link_libraries(trunc_prec_schunk blosc2_shared)
target_link_libraries(sum_openmp blosc2_shared)
target_link_libraries(zero_runlen blosc2_shared)


# have to copy blosc dlls on Windows
if(MSVC)
    add_custom_command(
            TARGET ${BENCH_EXE}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND}
            ARGS -E copy_if_different
            "${PROJECT_BINARY_DIR}/blosc/\$\(Configuration\)/blosc2.dll"
            "${CMAKE_CURRENT_BINARY_DIR}/\$\(Configuration\)/blosc2.dll")
elseif(MINGW)
    add_custom_command(
            TARGET ${BENCH_EXE}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND}
            ARGS -E copy_if_different
            "${PROJECT_BINARY_DIR}/blosc/libblosc2.dll"
            "${CMAKE_CURRENT_BINARY_DIR}/libblosc2.dll")
endif()

# tests
if(BUILD_TESTS)

    # The commented tests below take too much time to complete
    option(TEST_INCLUDE_BENCH_SHUFFLE_1 "Include ${BENCH_EXE} shuffle (1 thread) in the tests" ON)
    if(TEST_INCLUDE_BENCH_SHUFFLE_1)
        set(SHUFFLE_1_OPTS shuffle test 1)
        add_test(test_blosclz_shuffle_1 ${BENCH_EXE} blosclz ${SHUFFLE_1_OPTS})
        if(HAVE_LZ4)
            add_test(test_lz4_shuffle_1 ${BENCH_EXE} lz4 ${SHUFFLE_1_OPTS})
            # add_test(test_lz4hc_shuffle_1 ${BENCH_EXE} lz4hc ${SHUFFLE_1_OPTS})
        endif()
        if(HAVE_LIZARD)
            add_test(test_lizard_shuffle_1 ${BENCH_EXE} lizard ${SHUFFLE_1_OPTS})
        endif()
        if(HAVE_SNAPPY)
            add_test(test_snappy_shuffle_1 ${BENCH_EXE} snappy ${SHUFFLE_1_OPTS})
        endif()
        if(HAVE_ZLIB)
            # add_test(test_zlib_shuffle_1 ${BENCH_EXE} zlib ${SHUFFLE_1_OPTS})
        endif()
        if(HAVE_ZSTD)
            # add_test(test_zstd_shuffle_1 ${BENCH_EXE} zstd ${SHUFFLE_1_OPTS})
        endif()
    endif()

    option(TEST_INCLUDE_BENCH_SHUFFLE_N "Include ${BENCH_EXE} shuffle (multithread) in the tests" ON)
    if(TEST_INCLUDE_BENCH_SHUFFLE_N)
        set(SHUFFLE_N_OPTS shuffle test)
        add_test(test_blosclz_shuffle_n ${BENCH_EXE} blosclz ${SHUFFLE_N_OPTS})
        if(HAVE_LZ4)
            add_test(test_lz4_shuffle_n ${BENCH_EXE} lz4 ${SHUFFLE_N_OPTS})
            add_test(test_lz4hc_shuffle_n ${BENCH_EXE} lz4hc ${SHUFFLE_N_OPTS})
        endif()
        if(HAVE_LIZARD)
            add_test(test_lizard_shuffle_n ${BENCH_EXE} lizard ${SHUFFLE_N_OPTS})
        endif()
        if(HAVE_SNAPPY)
            add_test(test_snappy_shuffle_n ${BENCH_EXE} snappy ${SHUFFLE_N_OPTS})
        endif()
        if(HAVE_ZLIB)
            add_test(test_zlib_shuffle_n ${BENCH_EXE} zlib ${SHUFFLE_N_OPTS})
        endif()
        if(HAVE_ZSTD)
            add_test(test_zstd_shuffle_n ${BENCH_EXE} zstd ${SHUFFLE_N_OPTS})
        endif()
    endif()

    option(TEST_INCLUDE_BENCH_BITSHUFFLE_1 "Include ${BENCH_EXE} bitshuffle (1 thread) in the tests" ON)
    if(TEST_INCLUDE_BENCH_BITSHUFFLE_1)
        set(BITSHUFFLE_1_OPTS bitshuffle test 1)
        add_test(test_blosclz_bitshuffle_1 ${BENCH_EXE} blosclz ${BITSHUFFLE_1_OPTS})
        if(HAVE_LZ4)
            add_test(test_lz4_bitshuffle_1 ${BENCH_EXE} lz4 ${BITSHUFFLE_1_OPTS})
            # add_test(test_lz4hc_bitshuffle_1 ${BENCH_EXE} lz4hc ${BITSHUFFLE_1_OPTS})
        endif()
        if(HAVE_SNAPPY)
            add_test(test_snappy_bitshuffle_1 ${BENCH_EXE} snappy ${BITSHUFFLE_1_OPTS})
        endif()
        if(HAVE_ZLIB)
            # add_test(test_zlib_bitshuffle_1 ${BENCH_EXE} zlib ${BITSHUFFLE_1_OPTS})
        endif()
        if(HAVE_ZSTD)
            # add_test(test_zstd_bitshuffle_1 ${BENCH_EXE} zstd ${BITSHUFFLE_1_OPTS})
        endif()
    endif()

    option(TEST_INCLUDE_BENCH_BITSHUFFLE_N "Include ${BENCH_EXE} bitshuffle (multithread) in the tests" ON)
    if(TEST_INCLUDE_BENCH_BITSHUFFLE_N)
        set(BITSHUFFLE_N_OPTS bitshuffle test)
        add_test(test_blosclz_bitshuffle_n ${BENCH_EXE} blosclz ${BITSHUFFLE_N_OPTS})
        if(HAVE_LZ4)
            add_test(test_lz4_bitshuffle_n ${BENCH_EXE} lz4 ${BITSHUFFLE_N_OPTS})
            # add_test(test_lz4hc_bitshuffle_n ${BENCH_EXE} lz4hc ${BITSHUFFLE_N_OPTS})
        endif()
        if(HAVE_LIZARD)
            add_test(test_lizard_bitshuffle_n ${BENCH_EXE} lizard ${BITSHUFFLE_N_OPTS})
        endif()
        if(HAVE_SNAPPY)
            add_test(test_snappy_bitshuffle_n ${BENCH_EXE} snappy ${BITSHUFFLE_N_OPTS})
        endif()
        if(HAVE_ZLIB)
            # add_test(test_zlib_bitshuffle_n ${BENCH_EXE} zlib ${BITSHUFFLE_N_OPTS})
        endif()
        if(HAVE_ZSTD)
            # add_test(test_zstd_bitshuffle_n ${BENCH_EXE} zstd ${BITSHUFFLE_N_OPTS})
        endif()
    endif()

    option(TEST_INCLUDE_BENCH_SUITE "Include ${BENCH_EXE} suite in the tests" OFF)
    if(TEST_INCLUDE_BENCH_SUITE)
        add_test(test_hardsuite ${BENCH_EXE} blosclz shuffle suite)
    endif()

    option(TEST_INCLUDE_BENCH_DEBUGSUITE "Include ${BENCH_EXE} debugsuite in the tests" OFF)
    if(TEST_INCLUDE_BENCH_DEBUGSUITE)
        add_test(test_debugsuite ${BENCH_EXE} blosclz shuffle debugsuite)
    endif()

    option(TEST_INCLUDE_BENCH_DELTA "Include delta bench in the tests" ON)
    if(TEST_INCLUDE_BENCH_DELTA)
        add_test(test_bench_delta delta_schunk)
    endif()

    option(TEST_INCLUDE_BENCH_TRUNC_PREC "Include trunc bench in the tests" ON)
    if(TEST_INCLUDE_BENCH_TRUNC_PREC)
        add_test(test_bench_trunc_prec trunc_prec_schunk)
    endif()

    option(TEST_INCLUDE_BENCH_SUM_OPENMP "Include sum_openmp in the tests" OFF)
    if(TEST_INCLUDE_BENCH_SUM_OPENMP)
        add_test(test_bench_sum_openmp sum_openmp)
    endif()

    option(TEST_INCLUDE_BENCH_ZERO_RUNLEN "Include zero_runlen in the tests" ON)
    if(TEST_INCLUDE_BENCH_ZERO_RUNLEN)
        add_test(test_bench_zero_runlen zero_runlen)
    endif()

endif()
