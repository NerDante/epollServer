cmake_minimum_required(VERSION 2.8)

PROJECT(epoll_server)
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-std=c99)
ADD_SUBDIRECTORY(src) 
ADD_SUBDIRECTORY(example) 

