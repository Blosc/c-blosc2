cmake_minimum_required(VERSION 3.16)
project(exec_plan_contract LANGUAGES C)

# Small artifact-off fixture for iteration without building storage codecs.
# The installed-miniexpr fixture separately checks integration/static linking.
find_package(miniexpr CONFIG REQUIRED)
add_executable(exec_plan ../installed-miniexpr/exec-plan.c ../../blosc/b2nd-exec.c)
target_include_directories(exec_plan PRIVATE ../../blosc)
target_link_libraries(exec_plan PRIVATE miniexpr::core ${CMAKE_DL_LIBS})
target_compile_options(exec_plan PRIVATE -Wall -Wextra -Werror)
enable_testing()
add_test(NAME exec_plan COMMAND exec_plan)
add_executable(exec_plan_faults faults.c)
target_include_directories(exec_plan_faults PRIVATE ../../blosc)
target_link_libraries(exec_plan_faults PRIVATE miniexpr::core ${CMAKE_DL_LIBS})
target_compile_options(exec_plan_faults PRIVATE -Wall -Wextra -Werror)
add_test(NAME exec_plan_faults COMMAND exec_plan_faults)
