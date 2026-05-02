cmake_minimum_required(VERSION 3.16)
project(TowerOfHanoi LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(tower_of_hanoi
    src/main.cpp
    external/glad/src/gl.c
)

target_include_directories(tower_of_hanoi
    PRIVATE
        external/glad/include
)

find_package(glfw3 REQUIRED)

target_link_libraries(tower_of_hanoi
    PRIVATE
        glfw
)

if (UNIX)
    target_link_libraries(tower_of_hanoi PRIVATE dl)
endif()
