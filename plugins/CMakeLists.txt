# Blosc - Blocked Shuffling and Compression Library
#
# Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
# https://blosc.org
# License: BSD 3-Clause (see LICENSE.txt)
#
# See LICENSE.txt for details about copyright and rights to use.

add_subdirectory(codecs)
if(DEFINED ZFP_FETCHED)
    set(ZFP_FETCHED ${ZFP_FETCHED} PARENT_SCOPE)
endif()
if(DEFINED ZFP_SOURCE_DIR)
    set(ZFP_SOURCE_DIR ${ZFP_SOURCE_DIR} PARENT_SCOPE)
endif()
if(DEFINED ZFP_INSTALL_INCLUDEDIR)
    set(ZFP_INSTALL_INCLUDEDIR ${ZFP_INSTALL_INCLUDEDIR} PARENT_SCOPE)
endif()
add_subdirectory(tuners)
add_subdirectory(filters)

set(SOURCES ${SOURCES} PARENT_SCOPE)
