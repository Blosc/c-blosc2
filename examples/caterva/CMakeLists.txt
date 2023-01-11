# Blosc - Blocked Shuffling and Compression Library
#
# Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
# https://blosc.org
# License: BSD 3-Clause (see LICENSE.txt)
#
# See LICENSE.txt for details about copyright and rights to use.

file(GLOB SOURCES example_*.c)
if (NOT HAVE_PLUGINS)
    file(GLOB NEED_PLUGINS example_plugins_*.c)
    list(REMOVE_ITEM SOURCES ${NEED_PLUGINS})
endif()

foreach(source ${SOURCES})
    get_filename_component(target_name ${source} NAME_WE)
    set(target caterva_${target_name})
    add_executable(${target} ${target_name}.c)
    target_link_libraries(${target} blosc2_static ${LIBS})
endforeach(source)
