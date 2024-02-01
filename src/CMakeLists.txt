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
