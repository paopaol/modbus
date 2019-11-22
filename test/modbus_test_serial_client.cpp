#include "modbus_test_mocker.h"
#include <QSignalSpy>
#include <QTimer>

#define declare_app(name)                                                      \
  int argc = 1;                                                                \
  char *argv[] = {(char *)"test"};                                             \
  QCoreApplication name(argc, argv);

static const modbus::ServerAddress kServerAddress = 1;
static modbus::Request createReadCoilsRequest();

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

  modbus::Request request = createReadCoilsRequest();

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

  modbus::Request request = createReadCoilsRequest();
  modbus::ByteArray dataWitoutCrc = {kServerAddress,
                                     modbus::FunctionCode::kReadCoils, 1, 2, 3};
  modbus::ByteArray dataWithCrc = modbus::tool::appendCrc(dataWitoutCrc);

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

TEST(TestModbusSerialClient, setTimeout) {
  declare_app(app);

  {
    auto serialPort = new MockSerialPort();
    modbus::QSerialClient serialClient(serialPort);

    serialClient.setTimeout(2000);
    EXPECT_EQ(2000, serialClient.timeout());
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(TestModbusSerialClient, setRetryTimes) {
  declare_app(app);

  {
    auto serialPort = new MockSerialPort();
    modbus::QSerialClient serialClient(serialPort);

    serialClient.setRetryTimes(5);
    EXPECT_EQ(5, serialClient.retryTimes());
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(TestModbusSerialClient,
     sendRequestSuccessed_waitingForResponse_timeoutedWhenRetryTimesIsZero) {
  declare_app(app);

  {
    modbus::Request request = createReadCoilsRequest();

    auto serialPort = new MockSerialPort();
    serialPort->setupTestForWrite();

    modbus::QSerialClient serialClient(serialPort);

    serialClient.setRetryTimes(2);
    serialClient.setTimeout(500);
    QSignalSpy spy(&serialClient, &modbus::QSerialClient::requestFinished);

    EXPECT_CALL(*serialPort, open());
    /**
     * we set the retry times 2,so write() will be called twice
     */
    EXPECT_CALL(*serialPort, write(testing::_, testing::_)).Times(3);
    EXPECT_CALL(*serialPort, close());

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.sendRequest(request);
    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5 delay
    spy.wait(100000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    modbus::Response response =
        qvariant_cast<modbus::Response>(arguments.at(1));
    EXPECT_EQ(modbus::Error::kTimeout, response.error());
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(TestModbusSerialClient,
     sendRequestSuccessed_waitingForResponse_responseGetSuccessed) {
  declare_app(app);

  {
    modbus::Request request = createReadCoilsRequest();

    auto serialPort = new MockSerialPort();
    serialPort->setupTestForWriteRead();

    modbus::QSerialClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &modbus::QSerialClient::requestFinished);

    EXPECT_CALL(*serialPort, open());
    EXPECT_CALL(*serialPort, write(testing::_, testing::_));
    EXPECT_CALL(*serialPort, close());

    /**
     *In this test case, we simulate that the data returned by the serial port
     *is not all over the time. Therefore, it may be necessary to read it many
     *times before the data can be completely read. Here, we simulate that it
     *needs to read four times to finish reading.
     *
     * first time, only read server address (1 byte) //need more data
     * second time, read function code(1byte) + data(1 bytes) //need more data
     * thrd time, read data(1 byte)
     * fourth time, read crc(2 bytes)
     */
    modbus::ByteArray responseWithoutCrc = {
        kServerAddress, modbus::FunctionCode::kReadCoils,
        modbus::CoilAddress(0x01), modbus::Quantity(0x02)};

    modbus::ByteArray responseWithCrc =
        modbus::tool::appendCrc(responseWithoutCrc);

    QByteArray firstReadData;
    firstReadData.append(responseWithCrc[0]); /// server address

    QByteArray secondReadData;
    secondReadData.append(responseWithCrc[1]); /// function Code
    secondReadData.append(responseWithCrc[2]); /// coil address

    QByteArray thrdReadData;
    thrdReadData.append(responseWithCrc[3]); /// quantity

    QByteArray fourthReadData;
    fourthReadData.append(responseWithCrc[4])
        .append(responseWithCrc[5]); /// crc

    EXPECT_CALL(*serialPort, readAll())
        .Times(4)
        .WillOnce([&]() {
          QTimer::singleShot(10, [&]() { serialPort->readyRead(); });
          return firstReadData;
        })
        .WillOnce([&]() {
          QTimer::singleShot(10, [&]() { serialPort->readyRead(); });
          return secondReadData;
        })
        .WillOnce([&]() {
          QTimer::singleShot(10, [&]() { serialPort->readyRead(); });
          return thrdReadData;
        })
        .WillOnce([&]() { return fourthReadData; });

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.sendRequest(request);
    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5 delay
    spy.wait(200000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    modbus::Response response =
        qvariant_cast<modbus::Response>(arguments.at(1));
    EXPECT_EQ(modbus::Error::kNoError, response.error());
    EXPECT_EQ(modbus::FunctionCode::kReadCoils, response.functionCode());
    EXPECT_EQ(request.serverAddress(), response.serverAddress());
    EXPECT_EQ(response.data(), modbus::ByteArray({modbus::CoilAddress(0x01),
                                                  modbus::Quantity(0x02)}));
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(TestModbusSerialClient,
     sendRequestSuccessed_waitingForResponse_readSomethingThenErrorOcurr) {
  declare_app(app);

  {
    modbus::Request request = createReadCoilsRequest();

    auto serialPort = new MockSerialPort();
    serialPort->setupTestForWriteRead();

    modbus::QSerialClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &modbus::QSerialClient::requestFinished);

    EXPECT_CALL(*serialPort, open());
    EXPECT_CALL(*serialPort, write(testing::_, testing::_));
    EXPECT_CALL(*serialPort, close());

    QByteArray responseData;
    responseData.append(kServerAddress);

    EXPECT_CALL(*serialPort, readAll()).Times(1).WillOnce([&]() {
      QTimer::singleShot(10, [&]() { serialPort->error("read error"); });
      return responseData;
    });

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.sendRequest(request);
    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5 delay
    spy.wait(2000);
    /// if some error occured, can not get requestFinished signal
    EXPECT_EQ(spy.count(), 0);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

modbus::Request createReadCoilsRequest() {
  modbus::Request request;

  request.setServerAddress(kServerAddress);
  request.setFunctionCode(modbus::FunctionCode::kReadCoils);
  request.setDataChecker(MockReadCoilsDataChecker::newDataChecker());
  request.setData({1, 2, 3});
  return request;
}
