cmake_minimum_required(VERSION 3.12)
project(transparent_lockscreen)

set(CMAKE_CXX_STANDARD 17)

add_executable(transparent_lockscreen main.cpp)

find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

target_link_libraries(transparent_lockscreen
    ${GLFW_LIBRARIES}
    ${OPENGL_LIBRARIES}
    -lXrender
    -lX11
    -lpthread
    -lXtst
)
