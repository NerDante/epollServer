cmake_minimum_required(VERSION 2.8)
project(epoll_server)

include_directories(
    ../include
)
SET(LIBEPOLL_SERVER epollServer.c buffer.c)
ADD_LIBRARY(epserver SHARED ${LIBEPOLL_SERVER})
