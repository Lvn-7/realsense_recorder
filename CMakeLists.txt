cmake_minimum_required(VERSION 3.16)

project(realsense_recorder VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 5.12 REQUIRED COMPONENTS Core Widgets)
find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc imgcodecs videoio)

add_executable(realsense-recorder
    src/main.cpp
    src/camera_discovery.cpp
    src/camera_discovery.h
    src/main_window.cpp
    src/main_window.h
)

target_link_libraries(realsense-recorder PRIVATE
    Qt5::Core
    Qt5::Widgets
    ${OpenCV_LIBS}
)

target_compile_options(realsense-recorder PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS realsense-recorder RUNTIME DESTINATION bin)

