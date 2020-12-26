cmake_minimum_required(VERSION 2.8)
project(epoll_server)

SET(LIBEPOLL_SERVER epollServer.c buffer.c util.c)
ADD_LIBRARY(epserver SHARED ${LIBEPOLL_SERVER})
