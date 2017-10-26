cmake_minimum_required(VERSION 3.7)
project(solitaire_diamond)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES diamond-41.c)
add_executable(solitaire_diamond ${SOURCE_FILES})