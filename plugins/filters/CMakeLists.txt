# Blosc - Blocked Shuffling and Compression Library
#
# Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
# https://blosc.org
# License: BSD 3-Clause (see LICENSE.txt)
#
# See LICENSE.txt for details about copyright and rights to use.

add_subdirectory(ndcell)
add_subdirectory(ndmean)
add_subdirectory(bytedelta)
add_subdirectory(int_trunc)

set(SOURCES ${SOURCES} ${PROJECT_SOURCE_DIR}/plugins/filters/filters-registry.c PARENT_SCOPE)
