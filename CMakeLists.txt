cmake_minimum_required(VERSION 3.0)

project(modbus)



set(modbus_root_dir ${CMAKE_SOURCE_DIR})
if(MSVC)
    set(CMAKE_CXX_FLAGS "/EHsc")
    set(CMAKE_C_FLAGS " /EHsc")
else()
    set(CMAKE_CXX_FLAGS "-std=c++11")
    set(CMAKE_C_FLAGS "-std=c++11")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt5 REQUIRED Core SerialPort)



include_directories(".")
include_directories(include)

add_subdirectory(src)

enable_testing()
add_subdirectory(test)

add_subdirectory(examples)

# add_executable(main main.cpp)
# target_link_libraries(main Qt5::Core Qt5::SerialPort)
