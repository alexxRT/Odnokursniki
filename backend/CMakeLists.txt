cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(backend)


find_package(Boost REQUIRED)

add_executable(server server.cpp)
add_executable(client client.cpp)


target_link_libraries(server PUBLIC ${Boost_LIBRARY_DIRS})
target_link_libraries(client PUBLIC ${Boost_LIBRARY_DIRS})

target_include_directories(server PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(client PUBLIC ${Boost_INCLUDE_DIRS})
