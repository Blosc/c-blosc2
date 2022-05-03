get_filename_component(QCO_LOCAL_DIR ./ ABSOLUTE)
file(GLOB QCO_SRC_FILES ${QCO_LOCAL_DIR}/qco.*)
set(SOURCES ${SOURCES} ${QCO_SRC_FILES} PARENT_SCOPE)

if (BUILD_TESTS)
  add_executable(test_qco test_qco.c)
endif()
