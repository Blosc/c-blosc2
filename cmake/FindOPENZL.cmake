find_path(OPENZL_INCLUDE_DIR
  NAMES openzl.h
  PATH_SUFFIXES openzl
)

find_library(OPENZL_LIBRARY
  NAMES openzl
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OPENZL
  REQUIRED_VARS OPENZL_LIBRARY OPENZL_INCLUDE_DIR
)

if(OPENZL_FOUND AND NOT TARGET OpenZL::openzl)
  add_library(OpenZL::openzl UNKNOWN IMPORTED)
  set_target_properties(OpenZL::openzl PROPERTIES
    IMPORTED_LOCATION "${OPENZL_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${OPENZL_INCLUDE_DIR}"
  )
endif()
