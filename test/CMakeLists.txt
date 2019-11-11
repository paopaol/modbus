
enable_testing()

include_directories("../thrd/googletest/googletest/include")
set(gtest_force_shared_crt ON)
add_subdirectory(../thrd/googletest googletest)


include(GoogleTest)

add_executable(modbus_test "./modbus_test_bytearray_dump.cpp")
target_link_libraries(modbus_test gtest_main )
target_link_libraries(modbus_test gtest)
target_link_libraries(modbus_test Qt5::Core Qt5::SerialPort)
gtest_add_tests(modbus_test "./modbus_test_bytearray_dump.cpp" AUTO)
