#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTimer>
#include <gmock/gmock.h>
#include <modbus/base/modbus.h>
#include <modbus/base/modbus_tool.h>
#include <modbus/tools/modbus_serial_client.h>

TEST(TestData, dump_dumpByteArray_outputIsHexString) {
  uint8_t binary[5] = {0x01, 0x33, 0x4b, 0xab, 0x3b};
  modbus::ByteArray byteArray(binary, binary + 5);

  auto hexString = modbus::tool::dumpHex(byteArray);
  EXPECT_EQ(hexString, "01334bab3b");
}

class MockSerialPort : public modbus::AbstractSerialPort {
public:
  MockSerialPort(QObject *parent = nullptr)
      : modbus::AbstractSerialPort(parent) {}
  ~MockSerialPort() {}
  MOCK_METHOD(void, open, (), (override));
  MOCK_METHOD(void, close, (), (override));
  MOCK_METHOD(void, write, (const char *data, size_t size), (override));

  void setupDelegate() {
    ON_CALL(*this, open).WillByDefault([&]() { emit opened(); });
    ON_CALL(*this, close).WillByDefault([&]() { emit closed(); });
  }
  void setupOpenSuccessWriteFailedDelegate() {
    ON_CALL(*this, open).WillByDefault([&]() { emit opened(); });
    ON_CALL(*this, close).WillByDefault([&]() { emit closed(); });
    ON_CALL(*this, write).WillByDefault([&](const char *data, size_t size) {
      emit error("write serial failed");
    });
  }
  void setupOpenFailed() {
    ON_CALL(*this, open).WillByDefault([&]() {
      emit error("open serial failed");
    });
  }
};

class MockReadCoilsDataChecker {
public:
  static modbus::DataChecker::Result
  calculateRequestSize(size_t &size, const modbus::ByteArray &byteArray) {
    size = 4;
    return modbus::DataChecker::Result::kSizeOk;
  }
  static modbus::DataChecker::Result
  calculateResponseSize(size_t &size, const modbus::ByteArray &byteArray) {
    if (byteArray.size() < 1) {
      return modbus::DataChecker::Result::kNeedMoreData;
    }
    size_t bytes = byteArray[0];
    size = bytes + 1;
    return modbus::DataChecker::Result::kSizeOk;
  }
  static modbus::DataChecker newDataChecker() {
    modbus::DataChecker dataChecker;
    dataChecker.calculateRequestSize = [](size_t &size,
                                          const modbus::ByteArray &byteArray) {
      return MockReadCoilsDataChecker::calculateResponseSize(size, byteArray);
    };
    return dataChecker;
  }
};

TEST(TestData, modbusDataApiWorks) {
  modbus::any data = 1;
  const auto integer = modbus::any::any_cast<int>(data);
  EXPECT_EQ(1, integer);
}

TEST(TestPdu, modbusPduApiWorks) {
  auto dataChecker = MockReadCoilsDataChecker::newDataChecker();
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
}

TEST(TestPdu, modbusPdu_defaultFunctionCode_isInvalid) {
  modbus::Pdu pdu4;
  EXPECT_EQ(modbus::FunctionCode::kInvalidCode, pdu4.functionCode());
}

TEST(TestAdu, modbusAduApiWorks) {
  auto dataChecker = MockReadCoilsDataChecker::newDataChecker();
  modbus::Adu adu0(modbus::ServerAddress(1), modbus::FunctionCode::kReadCoils,
                   dataChecker);
  EXPECT_EQ(modbus::FunctionCode::kReadCoils, adu0.functionCode());

  modbus::Adu adu1(modbus::ServerAddress(1),
                   modbus::Pdu(modbus::FunctionCode::kReadCoils, dataChecker));
  EXPECT_EQ(modbus::FunctionCode::kReadCoils, adu1.functionCode());

  modbus::Adu adu2;

  adu2.setServerAddress(0x1);
  adu2.setFunctionCode(modbus::FunctionCode::kReadCoils);
  adu2.setData({1, 2, 3});
  size_t buildSize = adu2.marshalSize();
  EXPECT_EQ(buildSize, 1 + 1 + 3 /*serveraddress|function code|data*/);
  modbus::ByteArray buildData = adu2.marshalData();
  EXPECT_EQ(buildData,
            modbus::ByteArray({1, modbus::FunctionCode::kReadCoils, 1, 2, 3}));
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

TEST(TestModbusSerialClient, serialClientConstrct_IsClosed) {
  auto serialPort = new MockSerialPort();
  modbus::QSerialClient mockSerialClient(serialPort);
  EXPECT_EQ(mockSerialClient.isClosed(), true);
}

TEST(TestModbusSerialClient, serialClientIsClosed_openSerial_serialIsOpened) {
  int argc = 1;
  char *argv[] = {(char *)"test"};
  QCoreApplication app(argc, argv);
  auto serialPort = new MockSerialPort();
  serialPort->setupDelegate();
  {
    modbus::QSerialClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &modbus::QSerialClient::clientOpened);

    EXPECT_CALL(*serialPort, open());
    EXPECT_CALL(*serialPort, close());

    serialClient.open();
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(serialClient.isOpened(), true);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(TestModbusSerialClient, serialClientIsOpened_closeSerial_serialIsClosed) {
  int argc = 1;
  char *argv[] = {(char *)"test"};
  QCoreApplication app(argc, argv);
  auto serialPort = new MockSerialPort();
  serialPort->setupDelegate();
  {
    modbus::QSerialClient serialClient(serialPort);
    QSignalSpy spyOpen(&serialClient, &modbus::QSerialClient::clientOpened);
    QSignalSpy spyClose(&serialClient, &modbus::QSerialClient::clientClosed);

    EXPECT_CALL(*serialPort, open());
    EXPECT_CALL(*serialPort, close());

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(spyOpen.count(), 1);
    EXPECT_EQ(serialClient.isOpened(), true);

    // now close the client
    serialClient.close();
    EXPECT_EQ(spyClose.count(), 1);

    EXPECT_EQ(serialClient.isClosed(), true);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(TestModbusSerialClient, serialClientIsClosed_openSerial_serialOpenFailed) {
  int argc = 1;
  char *argv[] = {(char *)"test"};
  QCoreApplication app(argc, argv);
  auto serialPort = new MockSerialPort();
  serialPort->setupOpenFailed();
  {
    modbus::QSerialClient serialClient(serialPort);
    QSignalSpy spyOpen(&serialClient, &modbus::QSerialClient::errorOccur);

    EXPECT_CALL(*serialPort, open());

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(spyOpen.count(), 1);
    EXPECT_EQ(serialClient.isOpened(), false);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(TestModbusSerialClient, serialPortIsOpened_sendRequest_serialWriteFailed) {
  int argc = 1;
  char *argv[] = {(char *)"test"};
  QCoreApplication app(argc, argv);

  modbus::Request request;

  request.setServerAddress(1);
  request.setFunctionCode(modbus::FunctionCode::kReadCoils);
  request.setDataChecker(MockReadCoilsDataChecker::newDataChecker());
  request.setData({1, 2, 3});

  auto serialPort = new MockSerialPort();
  modbus::QSerialClient serialClient(serialPort);
  serialPort->setupOpenSuccessWriteFailedDelegate();
  {
    QSignalSpy spy(&serialClient, &modbus::QSerialClient::errorOccur);

    EXPECT_CALL(*serialPort, open());
    EXPECT_CALL(*serialPort, write(testing::_, testing::_));
    EXPECT_CALL(*serialPort, close());

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);
    serialClient.sendRequest(request);
    spy.wait(300);
    EXPECT_EQ(spy.count(), 1);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
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
