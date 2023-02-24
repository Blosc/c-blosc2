cmake_minimum_required(VERSION 3.13)

project(
  open_htj2k
  LANGUAGES CXX
  DESCRIPTION "Open source implementation of 15444-1 and 15444-15")
option(BUILD_SHARED_LIBS "Shared Libraries" ON)
enable_language(CXX)

# Check C++ version supported by compiler
include(CheckCXXCompilerFlag)
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  check_cxx_compiler_flag("/std:c++11" COMPILER_SUPPORTS_CXX11)
  check_cxx_compiler_flag("/std:c++14" COMPILER_SUPPORTS_CXX14)
  check_cxx_compiler_flag("/std:c++17" COMPILER_SUPPORTS_CXX17)
else()
  check_cxx_compiler_flag("-std=c++11" COMPILER_SUPPORTS_CXX11)
  check_cxx_compiler_flag("-std=c++14" COMPILER_SUPPORTS_CXX14)
  check_cxx_compiler_flag("-std=c++17" COMPILER_SUPPORTS_CXX17)
endif()

# Check Thread support
find_package(Threads)

# Check TIFF support
find_package(TIFF)

# Compiler settings
if(COMPILER_SUPPORTS_CXX17)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
elseif(COMPILER_SUPPORTS_CXX14)
  set(CMAKE_CXX_STANDARD 14)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
else()
  set(CMAKE_CXX_STANDARD 11)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

message(STATUS "${CMAKE_HOST_SYSTEM_PROCESSOR} ${CMAKE_CXX_COMPILER_ID}")

# Default BUILD_TYPE is "Release"
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
  message(STATUS "CMAKE_BUILD_TYPE is unspecified. Set it as Release.")
endif()

# Compiler optimization settings
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC") # MSVC
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd5051")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} /Od /DDEBUG /ZI")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} /Ox")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} /O2 /ZI")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang") # Clang
  if(NOT COMPILER_SUPPORTS_CXX17)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-attributes")
  endif()
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion -Wsign-conversion -Wunused-parameter -Wformat=0 -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer -fcolor-diagnostics"
  )
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -g")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} -O3 -g -DNDEBUG")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "IntelLLVM") # Intel oneAPI DPC++/C++ Compiler
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-attributes")
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion -Wsign-conversion -Wunused-parameter -Wformat=0 -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer -fcolor-diagnostics -gline-tables-only -fdebug-info-for-profiling")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -g")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} -O3 -g -DNDEBUG")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Intel") # Intel Compiler Classic
  message(STATUS "Intel compiler")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -diag-disable=10441 -Wno-conversion -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -p")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -ipo")
else() # GCC
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-attributes")
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion -Wsign-conversion -Wunused-parameter -Wformat=0 -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer -fdiagnostics-color=always -Wformat-overflow=0"
  )
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -g")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} -O3 -g -DNDEBUG")
endif()

if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^[xX]86_64$|^[aA][mM][dD]64$") # x86_64
  if(NOT MINGW)
    option(ENABLE_AVX2 "Enable the use of Intel AVX2 intrinsics" ON)
  endif()
  if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} /arch:AVX2 /EHsc /D \"_CRT_SECURE_NO_WARNINGS\"")
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  endif()
endif()

if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^[aA][rR][mM]64$|^[aA][aA][rR][cC][hH]64$") # aarch64
  option(ENABLE_ARM_NEON "Enable the use of ARM NEON intrinsics" ON)
  if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} /EHsc /D \"_CRT_SECURE_NO_WARNINGS\"")
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(APPLE)
      message(STATUS "Added -mcpu=apple-m1")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=apple-m1")
    else()
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a -mtune=cortex-a72")
    endif()
  else()
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -Wno-unused-parameter -Wno-unused-variable -Wformat-overflow=0 -Wno-unused-but-set-variable"
    )
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a -mtune=cortex-a72")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flax-vector-conversions")
  endif()
endif()

# add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/source/thirdparty/highway
# EXCLUDE_FROM_ALL)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Source files settings "libopen_htj2k" shared library target
add_library(open_htj2k SHARED ${SOURCES})
if (Threads_FOUND)
  message(STATUS "Thread library found")
  target_compile_definitions(open_htj2k PUBLIC "OPENHTJ2K_THREAD")
  message(STATUS "OPENHTJ2K_THREAD is set")
endif()

if (TIFF_FOUND)
  message(STATUS "libtiff found")
  message(STATUS "tiff include = ${TIFF_INCLUDE_DIR}")
  message(STATUS "tiff library = ${TIFF_LIBRARIES}")
  target_compile_definitions(open_htj2k PUBLIC "OPENHTJ2K_TIFF_SUPPORT")
  target_include_directories(open_htj2k PUBLIC ${TIFF_INCLUDE_DIR})
  target_link_libraries(open_htj2k PUBLIC ${TIFF_LIBRARIES})
  message(STATUS "OPENHTJ2K_TIFF_SUPPORT is set ")
endif()

if(ENABLE_ARM_NEON)
  message(STATUS "OPENHTJ2K_ENABLE_ARM_NEON is set")
  target_compile_definitions(open_htj2k PUBLIC "OPENHTJ2K_ENABLE_ARM_NEON")
elseif(ENABLE_AVX2)
  message(STATUS "OPENHTJ2K_TRY_AVX2 is set")
  target_compile_definitions(open_htj2k PUBLIC "OPENHTJ2K_TRY_AVX2")
endif()
target_include_directories(
  open_htj2k
  PRIVATE source/core/common
          source/core/codestream
          source/core/coding
          source/core/transform
          # source/core/interface
          source/core/jph
          # ${CMAKE_CURRENT_SOURCE_DIR}/source/thirdparty/highway
)
target_include_directories(open_htj2k INTERFACE source/core/interface)
set_target_properties(
  open_htj2k PROPERTIES OUTPUT_NAME
                        $<IF:$<CONFIG:Debug>,open_htj2k_d,open_htj2k_R>)
# target_link_libraries(open_htj2k PUBLIC ${CMAKE_THREAD_LIBS_INIT} PRIVATE hwy)
if (Threads_FOUND)
  target_link_libraries(open_htj2k PUBLIC ${CMAKE_THREAD_LIBS_INIT})
endif()
# INSTALL(TARGETS open_htj2k EXPORT open_htj2k INCLUDES DESTINATION include
# LIBRARY DESTINATION lib)
#
# INSTALL (DIRECTORY src/core/interface/ DESTINATION include/open_htj2k
# FILES_MATCHING PATTERN "*.h")
#
# INSTALL(FILES "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${PROJECT_NAME}.pc"
# DESTINATION lib/pkgconfig)

# find_package(OpenMP) if(OpenMP_FOUND) message(STATUS "OpenMP is found.")
# target_link_libraries(open_htj2k OpenMP::OpenMP_CXX) endif()
add_subdirectory(source/core/codestream)
add_subdirectory(source/core/coding)
add_subdirectory(source/core/transform)
add_subdirectory(source/core/interface)
add_subdirectory(source/core/jph)

# imgcmp target
add_executable(imgcmp)
add_subdirectory(source/apps/imgcmp)
target_include_directories(imgcmp PUBLIC source/apps/imgcmp/)