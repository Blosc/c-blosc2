How to update OpenHTJ2K
=======================

When introducing changes into the OpenHTJ2K directory, it is important to check if the new version of 
CMakeLists.txt has any change that affects the sources.

Mainly, it is important to keep the open_htj2k library being SHARED.
To achieve this, it is necessary that the next code line does not change:

    add_library(open_htj2k SHARED ${SOURCES})

The flag `SHARED` is necessary to make `blosc2_shared` library link correctly with `open_htj2k` library.

Moreover, it is important not to add the flags "-march=native" and "-mtune=native" to this type of lines:

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
