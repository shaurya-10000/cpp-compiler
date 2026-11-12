cmake_minimum_required(VERSION 3.16)
project(minicc CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Apply compiler-specific warning flags
if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

file(GLOB MINICC_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")
add_executable(minicc ${MINICC_SOURCES})

# ---- tests (run with: ctest --test-dir build) ----
enable_testing()
add_test(NAME suite_O0 COMMAND bash tests/run_tests.sh $<TARGET_FILE:minicc> -O0
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME suite_O1 COMMAND bash tests/run_tests.sh $<TARGET_FILE:minicc> -O1
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME diagnostics COMMAND bash tests/run_diag_tests.sh $<TARGET_FILE:minicc>
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
