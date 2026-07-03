cmake_minimum_required(VERSION 3.10)

project(ChessEngine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
endif()

file(GLOB SOURCES "src/*.cpp")

add_executable(engine ${SOURCES} tests/perft.cpp)
target_include_directories(engine PRIVATE src tests)
