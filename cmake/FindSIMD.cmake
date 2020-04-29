# Check if SSE/AVX instructions are available on the machine where
# the project is compiled.

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
   EXEC_PROGRAM(cat ARGS "/proc/cpuinfo" OUTPUT_VARIABLE CPUINFO)

   STRING(REGEX REPLACE "^.*(sse2).*$" "\\1" SSE_THERE ${CPUINFO})
   STRING(COMPARE EQUAL "sse2" "${SSE_THERE}" SSE2_TRUE)
   IF (SSE2_TRUE)
      set(SSE2_FOUND true CACHE BOOL "SSE2 available on host")
   ELSE ()
      set(SSE2_FOUND false CACHE BOOL "SSE2 available on host")
   ENDIF ()

   STRING(REGEX REPLACE "^.*(avx2).*$" "\\1" SSE_THERE ${CPUINFO})
   STRING(COMPARE EQUAL "avx2" "${SSE_THERE}" AVX2_TRUE)
   IF (AVX2_TRUE)
      set(AVX2_FOUND true CACHE BOOL "AVX2 available on host")
   ELSE ()
      set(AVX2_FOUND false CACHE BOOL "AVX2 available on host")
   ENDIF ()

ELSEIF(CMAKE_SYSTEM_NAME MATCHES "Darwin")
   EXEC_PROGRAM("/usr/sbin/sysctl -a | grep machdep.cpu.features" OUTPUT_VARIABLE CPUINFO)
   STRING(REGEX REPLACE "^.*[^S](SSE2).*$" "\\1" SSE_THERE ${CPUINFO})
   STRING(COMPARE EQUAL "SSE2" "${SSE_THERE}" SSE2_TRUE)
   IF (SSE2_TRUE)
      set(SSE2_FOUND true CACHE BOOL "SSE2 available on host")
   ELSE ()
      set(SSE2_FOUND false CACHE BOOL "SSE2 available on host")
   ENDIF ()

   EXEC_PROGRAM("/usr/sbin/sysctl -a | grep machdep.cpu.leaf7_features" OUTPUT_VARIABLE CPUINFO)
   STRING(REGEX REPLACE "^.*(AVX2).*$" "\\1" SSE_THERE ${CPUINFO})
   STRING(COMPARE EQUAL "AVX2" "${SSE_THERE}" AVX2_TRUE)
   IF (AVX2_TRUE)
      set(AVX2_FOUND true CACHE BOOL "AVX2 available on host")
   ELSE ()
      set(AVX2_FOUND false CACHE BOOL "AVX2 available on host")
   ENDIF ()

ELSEIF(CMAKE_SYSTEM_NAME MATCHES "Windows")
   # TODO.  For now supposing SSE2 is safe enough
   set(SSE2_FOUND   true  CACHE BOOL "SSE2 available on host")
   set(AVX2_FOUND   false CACHE BOOL "AVX2 available on host")
ELSE()
   set(SSE2_FOUND   true  CACHE BOOL "SSE2 available on host")
   set(AVX2_FOUND false CACHE BOOL "AVX2 available on host")
ENDIF()

if(NOT SSE2_FOUND)
      MESSAGE(STATUS "Could not find hardware support for SSE2 on this machine.")
endif()
if(NOT AVX2_FOUND)
      MESSAGE(STATUS "Could not find hardware support for AVX2 on this machine.")
endif()

mark_as_advanced(SSE2_FOUND AVX2_FOUND)
