cmake_policy(SET CMP0076 NEW)
target_sources(open_htj2k
    PRIVATE
    j2kmarkers.cpp
    codestream_source.cpp
    codestream_destination.cpp
)