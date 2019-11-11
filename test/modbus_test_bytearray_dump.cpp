#include <gtest/gtest.h>

#include <modbus/modbus.h>
#include <modbus/modbus_tool.h>
// #include <modbus/modbus_serial_client.h>

TEST(TestData, dump_dumpByteArray_outputIsHexString) {
  uint8_t binary[5] = {0x01, 0x33, 0x4b, 0xab, 0x3b};
  modbus::ByteArray byteArray(binary, binary + 5);

  auto hexString = modbus::tool::dumpHex(byteArray);
  EXPECT_EQ(hexString, "01334bab3b");
}

class MockReadCoilsDataChecker {
public:
  static size_t calculateResponseSize(modbus::DataChecker::Result &result,
                                      const modbus::ByteArray &byteArray) {
    result = modbus::DataChecker::Result::kNeedMoreData;
    return -1;
  }
};

class MockReadCoilsRequest {
public:
  MockReadCoilsRequest() {}
  ~MockReadCoilsRequest() {}
  modbus::ByteArray toByteArray() { return {}; }
  std::error_code fromByteArray(const modbus::ByteArray &byteArray) {
    return std::error_code();
  }
  std::string dumpReadable() { return ""; }

  inline bool operator==(const MockReadCoilsRequest &other) const {
    return byteArray_ == other.byteArray_;
  }

private:
  modbus::ByteArray byteArray_;
};

class MockReadCoilsResponse {
public:
  MockReadCoilsResponse() {}
  ~MockReadCoilsResponse() {}
  modbus::ByteArray toByteArray() { return {}; }
  std::error_code fromByteArray(const modbus::ByteArray &byteArray) {
    return std::error_code();
  }
  std::string dumpReadable() { return ""; }

  inline bool operator==(const MockReadCoilsResponse &other) const {
    return byteArray_ == other.byteArray_;
  }

private:
  modbus::ByteArray byteArray_;
};

TEST(TestData, modbusDataApiWorks) {
  modbus::any data = 1;
  const auto integer = modbus::any::any_cast<int>(data);
  EXPECT_EQ(1, integer);
}

TEST(TestPdu, modbusPduApiWorks) {
  modbus::DataChecker dataChecker;

  dataChecker.calculateRequestSize =
      std::bind(&MockReadCoilsDataChecker::calculateResponseSize,
                std::placeholders::_1, std::placeholders::_2);

  modbus::Pdu pdu0(modbus::FunctionCode::kReadCoils, dataChecker);
  auto fcode0 = pdu0.functionCode();
  EXPECT_EQ(fcode0, modbus::FunctionCode::kReadCoils);

  modbus::Pdu pdu1;
  pdu1.setFunctionCode(modbus::FunctionCode::kReadCoils);
  auto fcode = pdu1.functionCode();
  EXPECT_EQ(fcode, modbus::FunctionCode::kReadCoils);

  modbus::Pdu pdu2;
  pdu2.setFunctionCode(modbus::FunctionCode(modbus::FunctionCode::kReadCoils +
                                            modbus::Pdu::kExceptionByte));
  EXPECT_EQ(true, pdu2.isException());
  fcode = pdu2.functionCode();
  EXPECT_EQ(fcode, modbus::FunctionCode::kReadCoils);

  modbus::Pdu pdu4;
  EXPECT_EQ(modbus::FunctionCode::kInvalidCode, pdu4.functionCode());
}

TEST(TestAdu, modbusAduApiWorks) {
  modbus::DataChecker dataChecker;

  dataChecker.calculateRequestSize = [](modbus::DataChecker::Result &result,
                                        const modbus::ByteArray &byteArray) {
    return MockReadCoilsDataChecker::calculateResponseSize(result, byteArray);
  };

  modbus::Adu adu0(modbus::ServerAddress(1), modbus::FunctionCode::kReadCoils,
                   dataChecker);
  EXPECT_EQ(modbus::FunctionCode::kReadCoils, adu0.functionCode());

  modbus::Adu adu1(modbus::ServerAddress(1),
                   modbus::Pdu(modbus::FunctionCode::kReadCoils, dataChecker));
  EXPECT_EQ(modbus::FunctionCode::kReadCoils, adu1.functionCode());
}

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

// TEST(TestClient, sss) {
//   modbus::DataChecker readCoilsChecker;
//   readCoilsChecker.calculateRequiredSize =
//       std::bind(&MockReadCoilsDataChecker::calculateResponseSize,
//                 std::placeholders::_1, std::placeholders::_2);

//   modbus::Request request;

//   request.setServerAddress(1);
//   request.setFunctionCode(modbus::FunctionCode::kReadCoils);
//   request.setUserData(readCoilRequest);
//   request.setDataChecker(readCoilsChecker);
//   request.setData(readCoilsChecker.toByteArray());

//   modbus::Client *serialClient = modbus::Client::NewSerial(1);
//   serialClient->sendRequest(request);

//   QObject::connect(serialClient, &modbus::Client::requestFinished,
//                    [&](const modbus::Request &request, const modbus::Response
//                    &repsone) {
//                      if (response->isException()) {
//                        return;
//                      }
//                      switch (request->functionCode()) {
//                      case modbus::FunctionCode::kReadCoils: {
//                        modbus::BitsValue value;
//                        value =
//                        modbus::BitsValue::fromByteArray(response.data());

//                        bool stattus = value.get(1);
//                        break;
//                      }
//                      default: {
//                        processPrivateResponse(request, response);
//                      }
//                      }
//                    });
// }
