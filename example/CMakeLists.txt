cmake_minimum_required(VERSION 2.8)
project(test)

include_directories(
    ../include
)

link_directories(
    ../install
)
link_libraries(test epserever)
add_executable(test test_main.c)