set(src-files
    "./base/modbus_crc32.cpp"
    "./base/modbus_lrc.cpp"
    "./base/modbus_logger.cpp"
    "./base/modbus_logger.h"
    "./base/modbus_sixteen_bit_access_process.cpp"
    "./base/modbus_single_bit_access_process.cpp"
    "./base/buffer.cpp"
    "./tools/modbus_client.cpp"
    "./tools/modbus_reconnectable_iodevice.cpp"
    "./tools/modbus_client_p.h"
    "${modbus_root_dir}/include/modbus/tools/modbus_client.h"
    "./tools/modbus_qt_socket.cpp"
    "./tools/modbus_qt_serialport.cpp"
    "./tools/modbus_server.cpp"
    "${modbus_root_dir}/include/modbus/tools/modbus_server.h"
    "./tools/modbus_tcp_server.cpp"
    "./tools/modbus_serial_server.cpp"
    "./tools/modbus_server_p.h")

add_library(modbus ${src-files})
add_dependencies(modbus fmtlib)
target_include_directories(modbus PRIVATE ".")
target_include_directories(modbus PRIVATE "./base")
target_link_libraries(modbus PUBLIC Qt5::Core)
target_link_libraries(modbus PUBLIC Qt5::SerialPort)
target_link_libraries(modbus PUBLIC Qt5::Network)

target_include_directories(
  modbus PUBLIC "${PROJECT_BINARY_DIR}/thrd/fmtlib/include"
                "${PROJECT_SOURCE_DIR}/include")
target_link_directories(
  modbus PUBLIC ${PROJECT_BINARY_DIR}/thrd/fmtlib/${CMAKE_INSTALL_LIBDIR})
if(WIN32)
  target_link_libraries(modbus PUBLIC debug fmtd optimized fmt)
else()
  target_link_libraries(modbus PUBLIC fmt)
  target_link_libraries(modbus PUBLIC pthread)
endif()
