set(src-files
  "./base/modbus_crc32.cpp"
  "./tools/modbus_serial_client.cpp"
  "./tools/modbus_serial_client_p.h"
  "./tools/modbus_qt_serialport.cpp")

add_library(modbus ${src-files})
target_link_libraries(modbus PUBLIC Qt5::Core)
target_link_libraries(modbus PUBLIC Qt5::SerialPort)
