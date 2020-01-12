cmake_minimum_required(VERSION 3.0)

project(modbus)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(modbus_root_dir ${CMAKE_SOURCE_DIR})
if(MSVC)
    add_compile_options("/WX-")
    add_compile_options("/EHsc")
    add_compile_options("/wd4800")
    add_compile_definitions("_CRT_SECURE_NO_WARNINGS")
else()
    add_compile_options("-O0")
    add_compile_options("-std=c++11")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt5 REQUIRED Core SerialPort Network)


include_directories("./thrd/fmt/include")
add_subdirectory("./thrd/fmt")

include_directories(".")
include_directories(include)

add_subdirectory(src)

enable_testing()
add_subdirectory(test)

add_subdirectory(examples)
