#include "modbus_test_mocker.h"
#include <QSignalSpy>
#include <QTimer>

#define declare_app(name)                                                      \
  int argc = 1;                                                                \
  char *argv[] = {(char *)"test"};                                             \
  QCoreApplication name(argc, argv);

static const modbus::ServerAddress kServerAddress = 1;
static modbus::ByteArray appendCrc(const modbus::ByteArray &data);

TEST(TestModbusSerialClient, serialClientConstrct_defaultIsClosed) {
  auto serialPort = new MockSerialPort();
  modbus::QSerialClient mockSerialClient(serialPort);
  EXPECT_EQ(mockSerialClient.isClosed(), true);
}

TEST(TestModbusSerialClient, serialClientIsClosed_openSerial_serialIsOpened) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();
    modbus::QSerialClient serialClient(serialPort);
    serialPort->setupDelegate();

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
  declare_app(app);
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
  declare_app(app);
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
  declare_app(app);

  modbus::Request request;

  request.setServerAddress(1);
  request.setFunctionCode(modbus::FunctionCode::kReadCoils);
  request.setDataChecker(MockReadCoilsDataChecker::newDataChecker());
  request.setData({1, 2, 3});

  {
    auto serialPort = new MockSerialPort();
    modbus::QSerialClient serialClient(serialPort);
    serialPort->setupOpenSuccessWriteFailedDelegate();
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
    EXPECT_EQ(serialClient.isClosed(), true);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(TestModbusSerialClient,
     serialPortIsOpened_sendRequest_serialWriteSuccess) {
  declare_app(app);

  modbus::Request request;

  request.setServerAddress(kServerAddress);
  request.setFunctionCode(modbus::FunctionCode::kReadCoils);
  request.setDataChecker(MockReadCoilsDataChecker::newDataChecker());
  request.setData({1, 2, 3});
  modbus::ByteArray dataWitoutCrc = {kServerAddress,
                                     modbus::FunctionCode::kReadCoils, 1, 2, 3};
  modbus::ByteArray dataWithCrc = appendCrc(dataWitoutCrc);

  {
    auto serialPort = new MockSerialPort();
    modbus::QSerialClient serialClient(serialPort);
    serialPort->setupTestForWrite();
    QSignalSpy spy(&serialClient, &modbus::QSerialClient::requestFinished);

    EXPECT_CALL(*serialPort, open());
    EXPECT_CALL(*serialPort, write(testing::_, testing::_));
    EXPECT_CALL(*serialPort, close());

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.sendRequest(request);
    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5 delay
    spy.wait(300);
    auto sendoutData = serialPort->sendoutData();
    EXPECT_EQ(sendoutData, dataWithCrc);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

modbus::ByteArray appendCrc(const modbus::ByteArray &data) {
  uint16_t crc =
      modbus::tool::crc16_modbus((uint8_t *)(data.data()), data.size());
  auto dataWithCrc = data;
  /// first push low bit
  dataWithCrc.push_back(crc % 256);
  /// second push high bit
  dataWithCrc.push_back(crc / 256);
  return dataWithCrc;
}
