cmake_minimum_required(VERSION 3.25)
project(sql)

set(CMAKE_CXX_STANDARD 20)

add_executable(sql main.cpp include/ctpg.hpp include/utils.h)
target_sources(sql PUBLIC include)
target_include_directories(sql PRIVATE include)

find_package(fmt)
target_link_libraries(sql PRIVATE fmt::fmt)

find_package(Boost 1.78.0 REQUIRED)
if(Boost_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${Boost_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
else()
    message("BOOST NOT FOUND")
endif()