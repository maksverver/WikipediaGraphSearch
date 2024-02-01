cmake_minimum_required(VERSION 3.12...3.28)

project(
  WikipediaGraphSearch
  VERSION 0.1
  DESCRIPTION "Wikipedia shortest path search tool"
  LANGUAGES CXX
)

include(CTest)

find_package(SQLite3 REQUIRED)

find_package(PkgConfig)
pkg_check_modules(LIBXML2 libxml-2.0)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-std=c++17 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

add_subdirectory(src)
add_subdirectory(apps)
add_subdirectory(tests)
