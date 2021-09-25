find_path(ZLIBNG_INCLUDE_DIRS NAMES zlib-ng.h)

if(ZLIB_INCLUDE_DIRS)
  set(ZLIBNG_LIBRARY_DIRS ${ZLIBNG_INCLUDE_DIRS})

  if("${ZLIBNG_LIBRARY_DIRS}" MATCHES "/include$")
    # Strip off the trailing "/include" in the path.
    GET_FILENAME_COMPONENT(ZLIBNG_LIBRARY_DIRS ${ZLIBNG_LIBRARY_DIRS} PATH)
  endif("${ZLIBNG_LIBRARY_DIRS}" MATCHES "/include$")

  if(EXISTS "${ZLIBNG_LIBRARY_DIRS}/lib")
    set(ZLIBNG_LIBRARY_DIRS ${ZLIBNG_LIBRARY_DIRS}/lib)
  endif(EXISTS "${ZLIBNG_LIBRARY_DIRS}/lib")
endif()

find_library(ZLIBNG_LIBRARY NAMES z-ng libz-ng zlib-ng libz-ng.a)

set(ZLIBNG_LIBRARIES ${ZLIBNG_LIBRARY})
set(ZLIBNG_INCLUDE_DIRS ${ZLIBNG_INCLUDE_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZLIBNG DEFAULT_MSG ZLIBNG_LIBRARY ZLIBNG_INCLUDE_DIRS)

if(ZLIBNG_INCLUDE_DIRS AND ZLIBNG_LIBRARIES)
  set(ZLIBNG_FOUND TRUE)
else(ZLIBNG_INCLUDE_DIRS AND ZLIBNG_LIBRARIES)
  set(ZLIBNG_FOUND FALSE)
endif(ZLIBNG_INCLUDE_DIRS AND ZLIBNG_LIBRARIES)

if(ZLIBNG_FOUND)
  message(STATUS "Found zlib-ng: ${ZLIBNG_LIBRARIES}, ${ZLIBNG_INCLUDE_DIRS}")
endif(ZLIBNG_FOUND)

#[[
Copyright https://github.com/zlib-ng/minizip-ng, 2021

Condition of use and distribution are the same as zlib:

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgement in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
]]#
