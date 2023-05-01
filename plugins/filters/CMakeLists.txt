# Blosc - Blocked Shuffling and Compression Library
#
# Copyright (c) 2021  The Blosc Development Team <blosc@blosc.org>
# https://blosc.org
# License: BSD 3-Clause (see LICENSE.txt)
#
# See LICENSE.txt for details about copyright and rights to use.

add_subdirectory(ndcell)
add_subdirectory(ndmean)
add_subdirectory(bytedelta)

set(SOURCES ${SOURCES} ${PROJECT_SOURCE_DIR}/plugins/filters/filters-registry.c PARENT_SCOPE)
