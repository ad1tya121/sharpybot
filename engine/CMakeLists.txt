cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_COMPILER "C:/msys64/mingw64/bin/g++.exe")
set(CMAKE_C_COMPILER "C:/msys64/mingw64/bin/gcc.exe")

project(ChessEngine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")

file(GLOB SOURCES "src/*.cpp")

add_executable(engine ${SOURCES} tests/perft.cpp)
target_include_directories(engine PRIVATE src tests)