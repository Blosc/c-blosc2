# Find the Intel IPP (Integrated Performance Primitives)
#
# IPP_FOUND - System has IPP
# IPP_INCLUDE_DIRS - IPP include files directories
# IPP_LIBRARIES - The IPP libraries
#
# The environment variable IPPROOT is used to find the installation location.
# If the environment variable is not set we'll look for it in the default installation locations.
#
# Usage:
#
# find_package(IPP)
# if(IPP_FOUND)
#     target_link_libraries(TARGET ${IPP_LIBRARIES})
# endif()

find_path(IPP_ROOT_DIR
    include/ipp.h
    PATHS
        $ENV{IPPROOT}
        /opt/intel/compilers_and_libraries/linux/ipp
        /opt/intel/compilers_and_libraries/mac/ipp
        "C:/IntelSWTools/compilers_and_libraries/windows/ipp/"
        "C:/Program Files (x86)/IntelSWTools/compilers_and_libraries/windows/ipp"
        $ENV{HOME}/miniconda3
        $ENV{USERPROFILE}/miniconda3/Library
        "C:/Miniconda37-x64/Library" # Making AppVeyor happy
)

find_path(IPP_INCLUDE_DIR
        ipp.h
        PATHS
        ${IPP_ROOT_DIR}/include
        )

if(WIN32)
    set(IPP_SEARCH_LIB ippcore.lib)
    set(IPP_LIBS ippcore.lib ippdc.lib)
elseif(APPLE)
    set(IPP_SEARCH_LIB libippcore.a)
    set(IPP_LIBS libipps.a libippdc.a libippcore.a)
else() # Linux
    set(IPP_SEARCH_LIB libippcore.a)
    set(IPP_LIBS libippvm.a libipps.a libippdc.a libippcore.a)
endif()


find_path(IPP_LIB_SEARCHPATH
    ${IPP_SEARCH_LIB}
    PATHS
        ${IPP_ROOT_DIR}/lib/intel64
        ${IPP_ROOT_DIR}/lib
)

foreach (LIB ${IPP_LIBS})
    find_library(${LIB}_PATH ${LIB} PATHS ${IPP_LIB_SEARCHPATH})
    if(${LIB}_PATH)
        set(IPP_LIBRARIES ${IPP_LIBRARIES} ${${LIB}_PATH})
        message(STATUS "Found IPP ${LIB} in: ${${LIB}_PATH}")
    else()
        message(STATUS "Could not find ${LIB}: disabling IPP")
    endif()
endforeach()

set(IPP_INCLUDE_DIRS ${IPP_INCLUDE_DIR})
include_directories(${IPP_INCLUDE_DIRS})
