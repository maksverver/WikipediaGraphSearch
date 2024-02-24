cmake_minimum_required(VERSION 3.12...3.28)

project(
  WikipediaGraphSearch
  VERSION 0.2.0
  DESCRIPTION "Wikipedia shortest path search tool"
  LANGUAGES CXX
)

include(CTest)

find_package(SQLite3 REQUIRED)

find_package(PkgConfig)
pkg_check_modules(LIBXML2 libxml-2.0)

find_package(Python COMPONENTS Interpreter Development.Module)
find_package(pybind11 CONFIG)

find_package(Wt COMPONENTS Wt HTTP)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

add_subdirectory(src)
add_subdirectory(apps)
add_subdirectory(tests)
