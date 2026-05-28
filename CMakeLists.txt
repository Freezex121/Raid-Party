cmake_minimum_required(VERSION 3.20)
project(RaidParty VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(EMSCRIPTEN)
    add_subdirectory(C:/raylib/raylib ${CMAKE_SOURCE_DIR}/Builds/Web/raylib)
else()
    add_subdirectory(C:/raylib/raylib C:/raylib/raylib/build)
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.c)

add_executable(RaidParty ${SOURCES})
target_link_libraries(RaidParty PRIVATE raylib)
target_include_directories(RaidParty PRIVATE src)

if(WIN32 AND NOT EMSCRIPTEN)
    target_link_libraries(RaidParty PRIVATE winhttp)
endif()

if(NOT EMSCRIPTEN)
    add_custom_command(TARGET RaidParty POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:RaidParty>"
            "${CMAKE_SOURCE_DIR}/Builds/Standalone/$<TARGET_FILE_NAME:RaidParty>"
        COMMENT "Copying exe to Builds/Standalone/"
    )
endif()

if(EMSCRIPTEN)
    set_target_properties(RaidParty PROPERTIES
        LINK_FLAGS "--preload-file assets --shell-file shell.html -s MAX_WEBGL_VERSION=2 -s TOTAL_MEMORY=64MB"
        SUFFIX ".html"
    )
    target_link_libraries(RaidParty PRIVATE -sALLOW_MEMORY_GROWTH -sFORCE_FILESYSTEM -sASYNCIFY)
endif()
