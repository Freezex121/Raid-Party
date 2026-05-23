cmake_minimum_required(VERSION 3.20)
project(RaidParty VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(C:/raylib/raylib C:/raylib/raylib/build)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.c)

add_executable(RaidParty ${SOURCES})
target_link_libraries(RaidParty PRIVATE raylib)
target_include_directories(RaidParty PRIVATE src)
