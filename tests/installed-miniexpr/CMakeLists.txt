cmake_minimum_required(VERSION 3.16)
project(installed_miniexpr_consumer LANGUAGES C)

find_package(Blosc2 CONFIG REQUIRED)
if(NOT TARGET miniexpr::core)
    message(FATAL_ERROR "Blosc2 did not resolve its static miniexpr dependency")
endif()
add_executable(installed_miniexpr_consumer main.c)
# Prefer the candidate's headers even if a system Blosc2 is already installed.
set_property(TARGET installed_miniexpr_consumer PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
target_link_libraries(installed_miniexpr_consumer PRIVATE Blosc2::blosc2_static)
enable_testing()
add_test(NAME installed_miniexpr_consumer COMMAND installed_miniexpr_consumer)
