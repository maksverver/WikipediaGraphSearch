include_directories(../include)

add_executable(pipe-trick_test pipe-trick_test.cc)
target_link_libraries(pipe-trick_test PRIVATE common)
add_test(NAME pipe-trick_test COMMAND pipe-trick_test)

add_test(
  NAME python_wikipath_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMAND tests/python_wikipath_test.py
)
set_property(TEST python_wikipath_test PROPERTY
  ENVIRONMENT "PYTHONPATH=${CMAKE_BINARY_DIR}/src/")

add_test(
  NAME python_http_server_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMAND tests/python_http_server_test.py
)
set_property(TEST python_http_server_test PROPERTY
  ENVIRONMENT "PYTHONPATH=${CMAKE_BINARY_DIR}/src/:${CMAKE_SOURCE_DIR}/python/")
