cmake_minimum_required(VERSION 3.20)
project(lemport VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LEM_BUILD_APP "Build the SDL3 game frontend" ON)
option(LEM_BUILD_TESTS "Build unit tests" ON)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# Platform independent engine: file formats, simulation, rendering to indexed
# framebuffers. No SDL dependency so it can be reused by every frontend.
add_library(lemcore STATIC
  src/core/dat.cpp
  src/core/bitmap.cpp
  src/core/level.cpp
  src/core/graphicset.cpp
  src/core/levelrender.cpp
  src/core/maindat.cpp
)
target_include_directories(lemcore PUBLIC src/core)

add_executable(lemtool src/tools/lemtool.cpp src/tools/png.cpp)
target_link_libraries(lemtool PRIVATE lemcore)

if(LEM_BUILD_APP)
  find_package(SDL3 REQUIRED CONFIG)
endif()

if(LEM_BUILD_TESTS)
  enable_testing()
  add_executable(lemtests tests/test_main.cpp)
  target_link_libraries(lemtests PRIVATE lemcore)
  add_test(NAME lemtests COMMAND lemtests)
endif()
