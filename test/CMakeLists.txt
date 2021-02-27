find_package(Qt5 REQUIRED Test)

include_directories("../src/tools")
include_directories("../src")
include_directories("../src/base")

include(GoogleTest)

set(src-list
    "./modbus_test_bytearray_dump.cpp"
    "./modbus_test_data_checker.cpp"
    "./modbus_test_subarray.cpp"
    "./modbus_test_pdu.cpp"
    "./modbus_test_adu.cpp"
    "./modbus_test_rtu_frame_decoder_client_decode.cpp"
    "./modbus_test_mbap_frame_decoder_client_decode.cpp"
    "./modbus_test_mocker.h"
    "./modbus_test_single_bit_access.cpp"
    "./modbus_test_sixteen_value_test.cpp"
    "./modbus_test_sixteen_bit_access.cpp"
    "./modbus_test_sixteen_bit_access_process.cpp"
    "./modbus_test_single_bit_access_process.cpp"
    "./modbus_test_frame.cpp"
    "./modbus_test_request_response.cpp"
    "./modbus_test_serial_client.cpp"
    "./modbus_test_server.cpp")

add_executable(modbus_test ${src-list})
add_dependencies(modbus_test googletest)

target_include_directories(
  modbus_test PRIVATE "${PROJECT_BINARY_DIR}/thrd/googletest/include")
target_link_directories(
  modbus_test PRIVATE
  ${PROJECT_BINARY_DIR}/thrd/googletest/${CMAKE_INSTALL_LIBDIR})
target_link_libraries(modbus_test modbus)
target_link_libraries(modbus_test debug gtest_maind optimized gtest_main)
target_link_libraries(modbus_test debug gtestd optimized gtest)
target_link_libraries(modbus_test debug gmockd optimized gmock)
target_link_libraries(modbus_test Qt5::Core)
target_link_libraries(modbus_test Qt5::SerialPort)
target_link_libraries(modbus_test Qt5::Test)
gtest_add_tests(modbus_test "./modbus_test_bytearray_dump.cpp" AUTO)
