cmake_minimum_required(VERSION 2.8)
project(example)

include_directories(
    ../include
)

link_directories(
    ${PROJECT_SOURCE_DIR}/../install/lib
)
link_libraries(example epserver)
add_executable(example test_main.c)

