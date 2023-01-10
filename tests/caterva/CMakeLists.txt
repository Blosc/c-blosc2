# Copyright (C) 2018 Francesc Alted, Aleix Alcacer.
# Copyright (C) 2019-present Blosc Development team <blosc@blosc.org>
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# You may select, at your option, one of the above-listed licenses.

file(GLOB SOURCES test_*.c)

foreach(source ${SOURCES})
    get_filename_component(target_name ${source} NAME_WE)
    set(target caterva_${target_name})
    add_executable(${target} ${target_name}.c)
    target_link_libraries(${target} blosc2_static ${LIBS})
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES LABELS "caterva")
endforeach(source)
