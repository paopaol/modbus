add_executable(modbus_example_serialport_client
               modbus_example_serialport_client.cpp)
target_link_libraries(modbus_example_serialport_client modbus)

add_executable(serialport_client_sixteen_bit_access
               "./serialport_client_sixteen_bit_access.cpp")
target_link_libraries(serialport_client_sixteen_bit_access modbus)

add_executable(simple_modbus_client "./simple_modbus_client.cpp")
target_link_libraries(simple_modbus_client modbus)

add_executable(simple_modbus_server "./simple_modbus_server.cpp")
target_link_libraries(simple_modbus_server modbus)
