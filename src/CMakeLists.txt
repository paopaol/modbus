set(src-files
  "./tools/modbus_serial_client.cpp"
  "./tools/modbus_serial_client_p.h")

add_library(modbus ${src-files})
target_link_libraries(modbus Qt5::Core)
