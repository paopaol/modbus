#include "modbus_test_mocker.h"

TEST(TestRequest, modbusRequestApiWorks) {
  {
    modbus::Request request;
    request.setServerAddress(0x1);
    EXPECT_EQ(0x1, request.serverAddress());
  }
  {
    modbus::Request request;
    request.setFunctionCode(modbus::FunctionCode::kReadCoils);
    EXPECT_EQ(0x1, request.functionCode());
  }
  {
    modbus::Request request;
    request.setUserData(std::string("userdata"));
    EXPECT_EQ("userdata",
              modbus::any::any_cast<std::string>(request.userData()));
  }
  {
    modbus::Request request;
    request.setData({0x01, 0x02, 0x03});
    EXPECT_EQ(modbus::ByteArray({0x01, 0x02, 0x03}), request.data());
  }
}

TEST(TestResponse, modbusResponseApiWorks) {
  {
    modbus::Response response;

    response.setServerAddress(0x1);
    EXPECT_EQ(1, response.serverAddress());
  }
  {
    modbus::Response response;

    response.setFunctionCode(modbus::FunctionCode::kReadCoils);
    EXPECT_EQ(modbus::FunctionCode::kReadCoils, response.functionCode());
  }
}
