# Blosc - Blocked Shuffling and Compression Library
#
# Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
# https://blosc.org
# License: BSD 3-Clause (see LICENSE.txt)
#
# See LICENSE.txt for details about copyright and rights to use.

file(GLOB SOURCES test_*.c)

foreach (source ${SOURCES})
    get_filename_component(target_name ${source} NAME_WE)
    set(target b2nd_${target_name})
    add_executable(${target} ${target_name}.c)
    target_link_libraries(${target} blosc2_static ${LIBS})
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES LABELS "b2nd")
endforeach (source)
