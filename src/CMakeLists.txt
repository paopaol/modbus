set(src-files
  "./base/modbus_crc32.cpp"
  "./base/modbus_lrc.cpp"
  "./base/modbus_logger.cpp"
  "./base/modbus_logger.h"
  "./base/modbus_sixteen_bit_access_process.cpp"
  "./base/modbus_single_bit_access_process.cpp"
  "./tools/modbus_serial_client.cpp"
  "./tools/modbus_serial_client_p.h"
  "./tools/modbus_qt_serialport.cpp")

add_library(modbus ${src-files})
target_include_directories(modbus PRIVATE ".")
target_include_directories(modbus PRIVATE "./base")
target_link_libraries(modbus PUBLIC Qt5::Core)
target_link_libraries(modbus PUBLIC Qt5::SerialPort)
