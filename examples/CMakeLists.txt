
add_executable(modbus_example_serialport_client
  modbus_example_serialport_client.cpp
   "${CMAKE_SOURCE_DIR}/include/modbus/tools/modbus_serial_client.h"
  )
target_link_libraries(modbus_example_serialport_client modbus)
