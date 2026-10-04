cmake_minimum_required(VERSION 3.15)
project(video_stream C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(USE_EXTERNAL_GLFW OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(raylib)

set(PROTOCOL_SOURCES
    src/protocol.c
)

if(WIN32)
    list(APPEND PROTOCOL_SOURCES src/platform/socket_win.c)
else()
    list(APPEND PROTOCOL_SOURCES src/platform/socket_linux.c)
endif()

add_executable(client
    src/client.c
    ${PROTOCOL_SOURCES}
)

target_include_directories(client PRIVATE
    src
    src/platform
)

target_link_libraries(client PRIVATE
    raylib
    m
)

add_executable(server
    src/server.c
    ${PROTOCOL_SOURCES}
)

target_include_directories(server PRIVATE
    src
    src/platform
)

target_link_libraries(server PRIVATE
    m
)
