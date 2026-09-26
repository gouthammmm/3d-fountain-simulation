cmake_minimum_required(VERSION 3.16)
project(FlowingFountain LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)

add_executable(fountain main.cpp)
target_link_libraries(fountain PRIVATE OpenGL::GL OpenGL::GLU GLUT::GLUT)

if(MSVC)
    target_compile_options(fountain PRIVATE /W4)
else()
    target_compile_options(fountain PRIVATE -Wall -Wextra -Wpedantic)
endif()
