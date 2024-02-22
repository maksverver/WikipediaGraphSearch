include_directories(../include)

add_executable(index index.cc)
target_link_libraries(index PRIVATE parsing writing)

add_executable(inspect inspect.cc)
target_link_libraries(inspect PRIVATE reading)

add_executable(search search.cc)
target_link_libraries(search PRIVATE reading searching)

if (Wt_FOUND)
  add_executable(websearch websearch.cc)
  target_link_libraries(websearch PRIVATE reading searching Wt::Wt Wt::HTTP)
endif ()

if (LIBXML2_FOUND)
  add_executable(xml-stats xml-stats.cc)
  target_link_libraries(xml-stats PRIVATE parsing)
endif ()
