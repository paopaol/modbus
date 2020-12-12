cmake_minimum_required(VERSION 3.0)

project(modbus)

option(MODBUS_BUILD_TEST "build unit test" OFF)
option(MODBUS_BUILD_EXAMPLE "build examples" ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CONFIGURATION_TYPES ${CMAKE_BUILD_TYPE})

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(modbus_root_dir ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
  add_compile_options("/WX-")
  add_compile_options("/EHsc")
  add_compile_options("/wd4800")
  add_compile_options("/wd4267")
  add_compile_options("/wd4018")
  add_compile_definitions("_CRT_SECURE_NO_WARNINGS")
  add_compile_definitions("_SCL_SECURE_NO_WARNINGS")
else()
  add_compile_options("-std=c++11")
  include(GNUInstallDirs)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt5 REQUIRED Core SerialPort Network)

add_subdirectory("./thrd")

include_directories(".")
include_directories(include)

add_subdirectory(src)

if(MODBUS_BUILD_TEST)
  enable_testing()
  add_subdirectory(test)
endif()

if(MODBUS_BUILD_EXAMPLE)
  add_subdirectory(examples)
endif()

install(
  TARGETS modbus
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)

install(DIRECTORY include DESTINATION .)
