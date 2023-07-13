cmake_minimum_required(VERSION 3.25)
project(sql)

set(CMAKE_CXX_STANDARD 20)

add_executable(sql main.cpp include/ctpg.hpp include/logical.h include/keywords.h include/identifiers.h include/common.h include/numeral.h)
target_sources(sql PUBLIC include)
target_include_directories(sql PRIVATE include)

# increase constexpr step limits
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(sql PRIVATE -fconstexpr-steps=114514114514)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(sql PRIVATE -fconstexpr-ops-limit=114514114514)
else()
    message("Unsupported Compiler")
endif ()

find_package(fmt)
target_link_libraries(sql PRIVATE fmt::fmt)

find_package(Boost 1.78.0 REQUIRED)
if(Boost_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${Boost_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
else()
    message("BOOST NOT FOUND")
endif()