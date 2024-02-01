include_directories(../include)

add_executable(indexer indexer.cc)
target_link_libraries(indexer PRIVATE parsing writing)

add_executable(inspect inspect.cc)
target_link_libraries(inspect PRIVATE reading)

add_executable(search search.cc)
target_link_libraries(search PRIVATE reading searching)

if (WT AND WTHTTP)
  add_executable(websearch websearch.cc)
  target_link_libraries(websearch PRIVATE reading searching wt wthttp)
endif ()

if (LIBXML2_FOUND)
  add_executable(xml-stats xml-stats.cc)
  target_link_libraries(xml-stats PRIVATE parsing)
endif ()
