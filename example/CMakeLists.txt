cmake_minimum_required(VERSION 2.8)
project(echo_server)

include_directories(
    ../src
)

link_directories(
    ${PROJECT_SOURCE_DIR}/../install/lib
)
link_libraries(epserver pthread)
add_executable(echo_server echo_server.c)
add_executable(tcp2udp tcp2udp.c)
add_executable(multithread_echoserver multithread_server.c)

