include_directories(../include)

add_executable(pipe-trick_test pipe-trick_test.cc)
target_link_libraries(pipe-trick_test PRIVATE common)
add_test(NAME pipe-trick_test COMMAND pipe-trick_test)
