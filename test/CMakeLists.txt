

find_package(Qt5 REQUIRED Test)

include_directories("../thrd/googletest/googletest/include")
include_directories("../thrd/googletest/googlemock/include")
include_directories("../src/tools")
set(gtest_force_shared_crt ON)
add_subdirectory(../thrd/googletest googletest)


include(GoogleTest)

set(src-list "./modbus_test_bytearray_dump.cpp"
  "./modbus_test_adu.cpp"
  "./modbus_test_mocker.h"
  "./modbus_test_pdu.cpp"
  "./modbus_test_request_response.cpp"
  "./modbus_test_serial_client.cpp"
  "./modbus_test_subarray.cpp"
  "./modbus_test_functions.cpp"
  "${modbus_root_dir}/include/modbus/tools/modbus_serial_client.h")

add_executable(modbus_test ${src-list})
target_link_libraries(modbus_test modbus)
target_link_libraries(modbus_test gtest_main )
target_link_libraries(modbus_test gtest)
target_link_libraries(modbus_test gmock)
target_link_libraries(modbus_test Qt5::Core)
target_link_libraries(modbus_test Qt5::SerialPort)
target_link_libraries(modbus_test Qt5::Test)
gtest_add_tests(modbus_test "./modbus_test_bytearray_dump.cpp" AUTO)
