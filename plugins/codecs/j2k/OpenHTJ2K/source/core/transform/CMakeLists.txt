cmake_policy(SET CMP0076 NEW)
target_sources(open_htj2k
    PRIVATE
    idwt.cpp
    idwt_neon.cpp
    idwt_avx2.cpp
    fdwt.cpp
    fdwt_avx2.cpp
    fdwt_neon.cpp
    color.cpp
    color_avx2.cpp
    color_neon.cpp
)