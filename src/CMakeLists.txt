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
  "${CMAKE_SOURCE_DIR}/include/modbus/tools/modbus_server.h"
  "./tools/modbus_qt_socket.cpp"
  "./tools/modbus_qt_serialport.cpp"
  "./tools/modbus_server.cpp")

add_library(modbus ${src-files})
target_include_directories(modbus PRIVATE ".")
target_include_directories(modbus PRIVATE "./base")
target_link_libraries(modbus PUBLIC Qt5::Core)
target_link_libraries(modbus PUBLIC Qt5::SerialPort)
target_link_libraries(modbus PUBLIC Qt5::Network)
