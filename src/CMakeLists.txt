include_directories(../include)

add_library(common STATIC
  pipe-trick.cc
)

add_library(reading STATIC
  graph-reader.cc
  metadata-reader.cc
  reader.cc
)
target_link_libraries(reading PUBLIC common PRIVATE SQLite::SQLite3)

add_library(writing STATIC
  graph-writer.cc
  metadata-writer.cc
)
target_link_libraries(writing PUBLIC common PRIVATE SQLite::SQLite3)

add_library(searching STATIC
  searcher.cc
)
target_link_libraries(searching PUBLIC reading)

if (LIBXML2_FOUND)
  add_library(parsing STATIC parser.cc)

  target_link_libraries(parsing ${LIBXML2_LIBRARIES})
  target_include_directories(parsing PUBLIC ${LIBXML2_INCLUDE_DIRS})
  target_compile_options(parsing PUBLIC ${LIBXML2_CFLAGS_OTHER})
endif ()

if (pybind11_FOUND)
  # This library copies the list of sources from common/reading/searching above,
  # instead of depending on the common/reading/searching libraries, because the
  # shared library needs to be linked as position-independent code, which the
  # static libraries are not, by default, and changing that could affect
  # performance.
  python_add_library(wikipath MODULE
      python-module.cc
      pipe-trick.cc
      graph-reader.cc
      metadata-reader.cc
      reader.cc
      searcher.cc
      WITH_SOABI)
  target_link_libraries(wikipath PRIVATE pybind11::headers reading)
endif ()
