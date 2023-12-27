cmake_minimum_required(VERSION 3.27)
project(eps-proj)

#================================================================================
# Preamble
#================================================================================
set(CMAKE_VERBOSE_MAKEFILE      true)
set(CMAKE_CXX_STANDARD          23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

add_subdirectory(src)
