cmake_minimum_required(VERSION 2.8)
project(echo_server)

include_directories(
    ../include
)

link_directories(
    ${PROJECT_SOURCE_DIR}/../install/lib
)
link_libraries(echo_server epserver)
add_executable(echo_server echo_server.c)

