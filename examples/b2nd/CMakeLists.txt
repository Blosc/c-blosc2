# Blosc - Blocked Shuffling and Compression Library
#
# Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
# https://blosc.org
# License: BSD 3-Clause (see LICENSE.txt)
#
# See LICENSE.txt for details about copyright and rights to use.

file(GLOB SOURCES example_*.c)
list(REMOVE_ITEM SOURCES example_print_meta.c)
if (NOT HAVE_PLUGINS)
    file(GLOB NEED_PLUGINS example_plugins_*.c)
    list(REMOVE_ITEM SOURCES ${NEED_PLUGINS})
endif ()

foreach (source ${SOURCES})
    get_filename_component(target_name ${source} NAME_WE)
    set(target b2nd_${target_name})
    add_executable(${target} ${target_name}.c)
    target_link_libraries(${target} blosc_testing ${LIBS})
    if (${target_name} STREQUAL example_print_meta)
        add_test(NAME ${target} COMMAND ${target}
                "${PROJECT_BINARY_DIR}/examples/b2nd/example_big_float_frame.b2nd")
    else ()
        add_test(NAME ${target} COMMAND ${target})
    endif ()
    set_tests_properties(${target} PROPERTIES LABELS "b2nd")
endforeach (source)
