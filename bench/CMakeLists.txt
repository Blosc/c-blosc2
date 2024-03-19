# Blosc - Blocked Shuffling and Compression Library
#
# Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
# https://blosc.org
# License: BSD 3-Clause (see LICENSE.txt)
#
# See LICENSE.txt for details about copyright and rights to use.

# sources for main bench
set(SOURCES b2bench.c)
# other benchmarks
set(SOURCES_DELTA delta_schunk.c)
set(SOURCES_TRUNC_PREC trunc_prec_schunk.c)
set(SOURCES_SUM_OPENMP sum_openmp.c)
set(SOURCES_ZERO_RUNLEN zero_runlen.c)
set(SOURCES_CFRAME create_frame.c)
set(SOURCES_SFRAME sframe_bench.c)

add_subdirectory(b2nd)

# targets
add_executable(b2bench ${SOURCES})
add_executable(delta_schunk ${SOURCES_DELTA})
add_executable(trunc_prec_schunk ${SOURCES_TRUNC_PREC})
add_executable(sum_openmp ${SOURCES_SUM_OPENMP})
add_executable(zero_runlen ${SOURCES_ZERO_RUNLEN})
add_executable(create_frame ${SOURCES_CFRAME})
add_executable(sframe_bench ${SOURCES_SFRAME})
if(UNIX AND NOT APPLE)
    # cmake is complaining about LINK_PRIVATE in original PR
    # and removing it does not seem to hurt, so be it.
    # target_link_libraries(b2bench LINK_PRIVATE rt)
    target_link_libraries(b2bench rt)
    target_link_libraries(delta_schunk rt)
    target_link_libraries(trunc_prec_schunk rt)
    target_link_libraries(sum_openmp rt)
    target_link_libraries(zero_runlen rt)
    target_link_libraries(create_frame rt)
    target_link_libraries(sframe_bench rt)
endif()
if(UNIX)
    # Avoid a warning when using gcc without -fopenmp
    target_compile_options(sum_openmp PRIVATE "-Wno-unknown-pragmas")
endif()
target_link_libraries(b2bench blosc_testing)
target_link_libraries(delta_schunk blosc_testing)
target_link_libraries(trunc_prec_schunk blosc_testing)
target_link_libraries(sum_openmp blosc_testing)
target_link_libraries(zero_runlen blosc_testing)
target_link_libraries(create_frame blosc_testing)
target_link_libraries(sframe_bench blosc_testing)

# tests
if(BUILD_TESTS)

    # The commented tests below take too much time to complete
    option(TEST_INCLUDE_BENCH_SHUFFLE_1 "Include b2bench shuffle (1 thread) in the tests" ON)

    if(TEST_INCLUDE_BENCH_SHUFFLE_1)
        set(SHUFFLE_1_OPTS shuffle test 1)
        add_test(NAME test_blosclz_shuffle_1
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> blosclz ${SHUFFLE_1_OPTS})
        add_test(NAME test_lz4_shuffle_1
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> lz4 ${SHUFFLE_1_OPTS})
        # add_test(NAME test_lz4hc_shuffle_1
            # COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> lz4hc ${SHUFFLE_1_OPTS})
        if(HAVE_ZLIB)
            # add_test(NAME test_zlib_shuffle_1
                # COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> zlib ${SHUFFLE_1_OPTS})
        endif()
        if(HAVE_ZSTD)
            # add_test(NAME test_zstd_shuffle_1
                # COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> zstd ${SHUFFLE_1_OPTS})
        endif()
    endif()

    option(TEST_INCLUDE_BENCH_SHUFFLE_N "Include b2bench shuffle (multithread) in the tests" ON)
    if(TEST_INCLUDE_BENCH_SHUFFLE_N)
        set(SHUFFLE_N_OPTS shuffle test)
        add_test(NAME test_blosclz_shuffle_n
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> blosclz ${SHUFFLE_N_OPTS})
        add_test(NAME test_lz4_shuffle_n
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> lz4 ${SHUFFLE_N_OPTS})
        add_test(NAME test_lz4hc_shuffle_n
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> lz4hc ${SHUFFLE_N_OPTS})
        if(HAVE_ZLIB)
            add_test(NAME test_zlib_shuffle_n
                COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> zlib ${SHUFFLE_N_OPTS})
        endif()
        if(HAVE_ZSTD)
            add_test(NAME test_zstd_shuffle_n
                COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> zstd ${SHUFFLE_N_OPTS})
        endif()
    endif()

    option(TEST_INCLUDE_BENCH_BITSHUFFLE_1 "Include b2bench bitshuffle (1 thread) in the tests" ON)
    if(TEST_INCLUDE_BENCH_BITSHUFFLE_1)
        set(BITSHUFFLE_1_OPTS bitshuffle test 1)
        add_test(NAME test_blosclz_bitshuffle_1
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> blosclz ${BITSHUFFLE_1_OPTS})
        add_test(NAME test_lz4_bitshuffle_1
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> lz4 ${BITSHUFFLE_1_OPTS})
        # add_test(NAME test_lz4hc_bitshuffle_1
            # COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> lz4hc ${BITSHUFFLE_1_OPTS})
        if(HAVE_ZLIB)
            # add_test(NAME test_zlib_bitshuffle_1
                # COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> zlib ${BITSHUFFLE_1_OPTS})
        endif()
        if(HAVE_ZSTD)
            # add_test(NAME test_zstd_bitshuffle_1
                # COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> zstd ${BITSHUFFLE_1_OPTS})
        endif()
    endif()

    option(TEST_INCLUDE_BENCH_BITSHUFFLE_N "Include b2bench bitshuffle (multithread) in the tests" ON)
    if(TEST_INCLUDE_BENCH_BITSHUFFLE_N)
        set(BITSHUFFLE_N_OPTS bitshuffle test)
        add_test(NAME test_blosclz_bitshuffle_n
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> blosclz ${BITSHUFFLE_N_OPTS})
        add_test(NAME test_lz4_bitshuffle_n
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> lz4 ${BITSHUFFLE_N_OPTS})
        # add_test(NAME test_lz4hc_bitshuffle_n
            # COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> lz4hc ${BITSHUFFLE_N_OPTS})
        if(HAVE_ZLIB)
            # add_test(NAME test_zlib_bitshuffle_n
                # COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> zlib ${BITSHUFFLE_N_OPTS})
        endif()
        if(HAVE_ZSTD)
            # add_test(NAME test_zstd_bitshuffle_n
                # COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> zstd ${BITSHUFFLE_N_OPTS})
        endif()
    endif()

    option(TEST_INCLUDE_BENCH_SUITE "Include b2bench suite in the tests" OFF)
    if(TEST_INCLUDE_BENCH_SUITE)
        add_test(NAME test_hardsuite
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> blosclz shuffle suite)
    endif()

    option(TEST_INCLUDE_BENCH_DEBUGSUITE "Include b2bench debugsuite in the tests" OFF)
    if(TEST_INCLUDE_BENCH_DEBUGSUITE)
        add_test(NAME test_debugsuite
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:b2bench> blosclz shuffle debugsuite)
    endif()

    option(TEST_INCLUDE_BENCH_DELTA "Include delta bench in the tests" ON)
    if(TEST_INCLUDE_BENCH_DELTA)
        add_test(NAME test_bench_delta
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:delta_schunk>)
    endif()

    option(TEST_INCLUDE_BENCH_TRUNC_PREC "Include trunc bench in the tests" ON)
    if(TEST_INCLUDE_BENCH_TRUNC_PREC)
        add_test(NAME test_bench_trunc_prec
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:trunc_prec_schunk>)
    endif()

    option(TEST_INCLUDE_BENCH_SUM_OPENMP "Include sum_openmp in the tests" OFF)
    if(TEST_INCLUDE_BENCH_SUM_OPENMP)
        add_test(NAME test_bench_sum_openmp
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:sum_openmp>)
    endif()

    option(TEST_INCLUDE_BENCH_ZERO_RUNLEN "Include zero_runlen in the tests" ON)
    if(TEST_INCLUDE_BENCH_ZERO_RUNLEN)
        add_test(NAME test_bench_zero_runlen
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:zero_runlen>)
    endif()

endif()
