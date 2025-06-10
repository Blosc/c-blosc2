# Blosc - Blocked Shuffling and Compression Library
#
# Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
# https://blosc.org
# License: BSD 3-Clause (see LICENSE.txt)
#
# See LICENSE.txt for details about copyright and rights to use.

add_subdirectory(codecs)
add_subdirectory(tuners)
add_subdirectory(filters)

set(SOURCES ${SOURCES} PARENT_SCOPE)
