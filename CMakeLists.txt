cmake_minimum_required(VERSION 3.25)
project(sql)

set(CMAKE_CXX_STANDARD 20)

add_executable(sql main.cpp include/ctpg.hpp)
target_sources(sql PUBLIC include)
target_include_directories(sql PRIVATE include)
