add_subdirectory(ndlz)
add_subdirectory(zfp)

set(SOURCES ${SOURCES} ${PROJECT_SOURCE_DIR}/plugins/codecs/codecs-registry.c PARENT_SCOPE)
