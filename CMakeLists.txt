cmake_minimum_required(VERSION 3.28)
project(rtes C)

set(CMAKE_C_STANDARD 17)

add_executable(rtes main.c)
target_link_libraries(rtes m)
