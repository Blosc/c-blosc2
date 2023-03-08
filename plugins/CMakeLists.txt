# Blosc - Blocked Shuffling and Compression Library
#
# Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
# https://blosc.org
# License: BSD 3-Clause (see LICENSE.txt)
#
# See LICENSE.txt for details about copyright and rights to use.

add_subdirectory(codecs)
add_subdirectory(filters)

set(SOURCES ${SOURCES} ${PROJECT_SOURCE_DIR}/plugins/plugin_utils.c PARENT_SCOPE)
