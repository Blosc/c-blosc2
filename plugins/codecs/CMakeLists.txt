if(NOT DEACTIVATE_J2K)
    add_subdirectory(j2k)
endif()
add_subdirectory(ndlz)
add_subdirectory(zfp)

set(SOURCES ${SOURCES} ../plugins/codecs/codecs-registry.c PARENT_SCOPE)