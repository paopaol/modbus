cmake_minimum_required(VERSION 3.0)

project(modbus)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED Core SerialPort)


include_directories(".")
add_executable(main main.cpp)
target_link_libraries(main Qt5::Core Qt5::SerialPort)
