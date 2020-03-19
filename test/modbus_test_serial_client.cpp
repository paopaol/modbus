#include "modbus_test_mocker.h"
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <modbus/base/single_bit_access.h>
#include <modbus/base/sixteen_bit_access.h>
#include <modbus_frame.h>

struct Session {
  modbus::Request request;
  modbus::ByteArray requestRaw;
  modbus::Response response;
  modbus::ByteArray responseRaw;
};

#define declare_app(name)                                                      \
  int argc = 1;                                                                \
  char *argv[] = {(char *)"test"};                                             \
  QCoreApplication name(argc, argv);

static const modbus::Address kStartAddress = 10;
static const modbus::Quantity kQuantity = 3;
static const modbus::ServerAddress kServerAddress = 1;
static const modbus::ServerAddress kBadServerAddress = 0x11;
static modbus::Request createSingleBitAccessRequest();
static modbus::Request createBrocastRequest();
template <modbus::TransferMode mode>
static void createReadCoils(modbus::ServerAddress serverAddress,
                            modbus::Address startAddress,
                            modbus::Quantity quantity, Session &session);
static void configMockSerialPortWithReponse(MockSerialPort *mocker,
                                            const modbus::ByteArray &response);
static QByteArray buildResponseArray(const modbus::ByteArray &response);

static void clientConstruct_defaultIsClosed(modbus::TransferMode transferMode) {
  auto serialPort = new MockSerialPort();
  modbus::QModbusClient mockSerialClient(serialPort);
  EXPECT_EQ(mockSerialClient.transferMode(), modbus::TransferMode::kRtu);
  mockSerialClient.setTransferMode(transferMode);
  EXPECT_EQ(mockSerialClient.transferMode(), transferMode);
  EXPECT_EQ(mockSerialClient.isClosed(), true);
}

TEST(ModbusClient, ClientConstruct_defaultIsClosed) {
  clientConstruct_defaultIsClosed(modbus::TransferMode::kRtu);
  clientConstruct_defaultIsClosed(modbus::TransferMode::kAscii);
}

static void
clientIsClosed_openDevice_clientIsOpened(modbus::TransferMode transferMode) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();
    modbus::QModbusClient serialClient(serialPort);
    serialClient.setTransferMode(transferMode);
    serialPort->setupDelegate();

    QSignalSpy spy(&serialClient, &modbus::QModbusClient::clientOpened);

    EXPECT_CALL(*serialPort, open());
    EXPECT_CALL(*serialPort, close());

    serialClient.open();
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(serialClient.isOpened(), true);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient, clientIsClosed_openDevice_clientIsOpened) {
  clientIsClosed_openDevice_clientIsOpened(modbus::TransferMode::kRtu);
  clientIsClosed_openDevice_clientIsOpened(modbus::TransferMode::kAscii);
}

TEST(ModbusSerialClient, clientIsClosed_openSerial_retry4TimesFailed) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();
    modbus::QModbusClient serialClient(serialPort);
    serialPort->setupOpenFailed();

    QSignalSpy spy(&serialClient, &modbus::QModbusClient::clientOpened);

    EXPECT_CALL(*serialPort, open()).Times(5);
    EXPECT_CALL(*serialPort, close()).WillRepeatedly(testing::Invoke([&]() {
      serialPort->closed();
    }));

    serialClient.setOpenRetryTimes(4, 1000);
    serialClient.open();
    spy.wait(8000);
    EXPECT_EQ(spy.count(), 0);
    EXPECT_EQ(serialClient.isOpened(), false);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient, clientIsOpened_closeSerial_clientIsClosed) {
  declare_app(app);
  auto serialPort = new MockSerialPort();
  serialPort->setupDelegate();
  {
    modbus::QModbusClient serialClient(serialPort);
    QSignalSpy spyOpen(&serialClient, &modbus::QModbusClient::clientOpened);
    QSignalSpy spyClose(&serialClient, &modbus::QModbusClient::clientClosed);

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

TEST(ModbusSerialClient, clientIsClosed_openSerial_clientOpenFailed) {
  declare_app(app);
  auto serialPort = new MockSerialPort();
  serialPort->setupOpenFailed();

  {
    modbus::QModbusClient serialClient(serialPort);
    QSignalSpy spyOpen(&serialClient, &modbus::QModbusClient::errorOccur);

    EXPECT_CALL(*serialPort, open());
    EXPECT_CALL(*serialPort, close()).WillRepeatedly(testing::Invoke([&]() {
      serialPort->closed();
    }));

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(spyOpen.count(), 1);
    EXPECT_EQ(serialClient.isOpened(), false);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient, clientOpened_sendRequest_clientWriteFailed) {
  declare_app(app);

  Session session;
  createReadCoils<modbus::TransferMode::kRtu>(kServerAddress, kStartAddress,
                                              kQuantity, session);

  {
    auto serialPort = new MockSerialPort();
    modbus::QModbusClient serialClient(serialPort);
    serialPort->setupOpenSuccessWriteFailedDelegate();

    QSignalSpy spy(&serialClient, &modbus::QModbusClient::errorOccur);

    EXPECT_CALL(*serialPort, open());
    EXPECT_CALL(*serialPort, write(testing::_, testing::_));
    EXPECT_CALL(*serialPort, close());

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);
    serialClient.sendRequest(session.request);
    serialClient.sendRequest(session.request);
    serialClient.sendRequest(session.request);
    EXPECT_EQ(serialClient.pendingRequestSize(), 3);

    spy.wait(300);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(serialClient.isClosed(), true);
    /// after serial client closed, no pending request exists
    EXPECT_EQ(serialClient.pendingRequestSize(), 0);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient, clientIsOpened_sendRequest_clientWriteSuccess) {
  declare_app(app);

  Session session;
  createReadCoils<modbus::TransferMode::kRtu>(kServerAddress, kStartAddress,
                                              kQuantity, session);

  {
    auto serialPort = new MockSerialPort();
    modbus::QModbusClient serialClient(serialPort);
    serialPort->setupTestForWrite();

    QSignalSpy spy(&serialClient, &modbus::QModbusClient::requestFinished);

    EXPECT_CALL(*serialPort, open());
    EXPECT_CALL(*serialPort, write(testing::_, testing::_));
    EXPECT_CALL(*serialPort, close());

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.sendRequest(session.request);
    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5 delay
    spy.wait(300);
    auto sendoutData = serialPort->sendoutData();
    EXPECT_EQ(sendoutData, session.requestRaw);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient, setTimeout) {
  declare_app(app);

  {
    auto serialPort = new MockSerialPort();
    modbus::QModbusClient serialClient(serialPort);

    serialClient.setTimeout(2000);
    EXPECT_EQ(2000, serialClient.timeout());
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient, setRetryTimes) {
  declare_app(app);

  {
    auto serialPort = new MockSerialPort();
    modbus::QModbusClient serialClient(serialPort);

    serialClient.setRetryTimes(5);
    EXPECT_EQ(5, serialClient.retryTimes());
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient,
     sendRequestSuccessed_waitingForResponse_timeoutedWhenRetryTimesIsZero) {
  declare_app(app);

  {
    Session session;
    createReadCoils<modbus::TransferMode::kRtu>(kServerAddress, kStartAddress,
                                                kQuantity, session);

    auto serialPort = new MockSerialPort();
    serialPort->setupTestForWrite();

    modbus::QModbusClient serialClient(serialPort);

    serialClient.setRetryTimes(2);
    serialClient.setTimeout(500);
    QSignalSpy spy(&serialClient, &modbus::QModbusClient::requestFinished);

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
    serialClient.sendRequest(session.request);
    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5 delay
    spy.wait(100000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    modbus::Response response =
        qvariant_cast<modbus::Response>(arguments.at(1));
    EXPECT_EQ(modbus::Error::kTimeout, response.error());
    EXPECT_EQ(2, serialClient.retryTimes());
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient,
     sendRequestSuccessed_waitingForResponse_responseGetSuccessed) {
  declare_app(app);

  {
    Session session;
    createReadCoils<modbus::TransferMode::kRtu>(kServerAddress, kStartAddress,
                                                kQuantity, session);

    auto serialPort = new MockSerialPort();
    serialPort->setupTestForWriteRead();

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &modbus::QModbusClient::requestFinished);

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
    modbus::ByteArray responseWithCrc = session.responseRaw;

    QByteArray firstReadData;
    firstReadData.append(responseWithCrc[0]); /// server address

    QByteArray secondReadData;
    secondReadData.append(responseWithCrc[1]); /// function Code
    secondReadData.append(responseWithCrc[2]); /// bytes number
    secondReadData.append(responseWithCrc[3]); /// value

    QByteArray thrdReadData;
    thrdReadData.append(responseWithCrc[4]); /// crc1

    QByteArray fourthReadData;
    fourthReadData.append(responseWithCrc[5]); /// crc2

    EXPECT_CALL(*serialPort, readAll())
        .Times(4)
        .WillOnce(testing::Invoke([&]() {
          QTimer::singleShot(10, [&]() { serialPort->readyRead(); });
          return firstReadData;
        }))
        .WillOnce(testing::Invoke([&]() {
          QTimer::singleShot(10, [&]() { serialPort->readyRead(); });
          return secondReadData;
        }))
        .WillOnce(testing::Invoke([&]() {
          QTimer::singleShot(10, [&]() { serialPort->readyRead(); });
          return thrdReadData;
        }))
        .WillOnce(testing::Invoke([&]() { return fourthReadData; }));

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.sendRequest(session.request);
    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5 delay
    spy.wait(200000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    modbus::Response response =
        qvariant_cast<modbus::Response>(arguments.at(1));
    EXPECT_EQ(modbus::Error::kNoError, response.error());
    EXPECT_EQ(modbus::FunctionCode::kReadCoils, response.functionCode());
    EXPECT_EQ(session.request.serverAddress(), response.serverAddress());
    EXPECT_EQ(response.data(), session.response.data());
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient,
     sendRequestSuccessed_waitingForResponse_responseCrcError) {
  declare_app(app);

  {
    Session session;
    createReadCoils<modbus::TransferMode::kRtu>(kServerAddress, kStartAddress,
                                                kQuantity, session);
    auto serialPort = new MockSerialPort();
    serialPort->setupTestForWriteRead();

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &modbus::QModbusClient::requestFinished);

    EXPECT_CALL(*serialPort, open());
    EXPECT_CALL(*serialPort, write(testing::_, testing::_));
    EXPECT_CALL(*serialPort, close());

    session.responseRaw[session.responseRaw.size() - 1] = 0x00;
    QByteArray qarray((const char *)session.responseRaw.data(),
                      session.responseRaw.size());

    EXPECT_CALL(*serialPort, readAll())
        .Times(1)
        .WillOnce(testing::Invoke([&]() { return qarray; }));

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.sendRequest(session.request);
    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5 delay
    spy.wait(200000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    modbus::Response response =
        qvariant_cast<modbus::Response>(arguments.at(1));
    EXPECT_EQ(modbus::Error::kStorageParityError, response.error());
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient,
     sendRequestSuccessed_waitingForResponse_responseExpection) {
  declare_app(app);

  {
    Session session;
    createReadCoils<modbus::TransferMode::kRtu>(kServerAddress, kStartAddress,
                                                kQuantity, session);
    modbus::FunctionCode functionCode = modbus::FunctionCode(
        session.response.functionCode() | modbus::Pdu::kExceptionByte);
    session.response.setFunctionCode(functionCode);
    session.response.setError(modbus::Error::kSlaveDeviceBusy);
    session.responseRaw =
        modbus::marshalRtuFrame(session.response.marshalAduWithoutCrc());

    auto serialPort = new MockSerialPort();
    serialPort->setupTestForWriteRead();

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &modbus::QModbusClient::requestFinished);

    EXPECT_CALL(*serialPort, open());
    EXPECT_CALL(*serialPort, write(testing::_, testing::_));
    EXPECT_CALL(*serialPort, close());

    modbus::ByteArray responseWithoutCrc = {
        kServerAddress,
        /**
         * use modbus::FunctionCode::kReadCoils | modbus::Pdu::kExceptionByte
         * simulated exception return
         */
        modbus::FunctionCode::kReadCoils | modbus::Pdu::kExceptionByte,
        static_cast<uint8_t>(modbus::Error::kSlaveDeviceBusy)};

    modbus::ByteArray responseWithCrc =
        modbus::tool::appendCrc(responseWithoutCrc);
    QByteArray qarray((const char *)responseWithCrc.data(),
                      responseWithCrc.size());

    EXPECT_CALL(*serialPort, readAll())
        .Times(1)
        .WillOnce(testing::Invoke([&]() { return qarray; }));

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.sendRequest(session.request);
    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5 delay
    spy.wait(200000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    modbus::Response response =
        qvariant_cast<modbus::Response>(arguments.at(1));
    EXPECT_EQ(modbus::Error::kSlaveDeviceBusy, response.error());
    EXPECT_EQ(true, response.isException());
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient,
     sendRequestSuccessed_responseIsFromBadServerAddress_timeout) {
  declare_app(app);

  {
    Session session;
    createReadCoils<modbus::TransferMode::kRtu>(kServerAddress, kStartAddress,
                                                kQuantity, session);
    modbus::FunctionCode functionCode = modbus::FunctionCode(
        session.response.functionCode() | modbus::Pdu::kExceptionByte);
    session.response.setServerAddress(0x00);
    session.response.setFunctionCode(functionCode);
    session.response.setError(modbus::Error::kTimeout);
    session.responseRaw =
        modbus::marshalRtuFrame(session.response.marshalAduWithoutCrc());

    auto serialPort = new MockSerialPort();
    serialPort->setupTestForWriteRead();

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &modbus::QModbusClient::requestFinished);

    EXPECT_CALL(*serialPort, open());
    EXPECT_CALL(*serialPort, write(testing::_, testing::_));
    EXPECT_CALL(*serialPort, close());

    QByteArray qarray((const char *)session.responseRaw.data(),
                      session.responseRaw.size());
    EXPECT_CALL(*serialPort, readAll())
        .Times(1)
        .WillOnce(testing::Invoke([&]() { return qarray; }));

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.sendRequest(session.request);
    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5 delay
    spy.wait(200000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    modbus::Response response =
        qvariant_cast<modbus::Response>(arguments.at(1));
    EXPECT_EQ(modbus::Error::kTimeout, response.error());
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient, sendBrocast_gotResponse_discardIt) {
  declare_app(app);

  {
    modbus::Request request = createBrocastRequest();

    auto serialPort = new MockSerialPort();
    serialPort->setupTestForWriteRead();

    modbus::ByteArray responseWithoutCrc = {kServerAddress,
                                            modbus::FunctionCode::kReadCoils,
                                            0x01, 0x05 /*b 0000 0101*/};
    modbus::ByteArray responseWithCrc =
        modbus::tool::appendCrc(responseWithoutCrc);
    QByteArray qarray((const char *)responseWithCrc.data(),
                      responseWithCrc.size());

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &modbus::QModbusClient::requestFinished);

    EXPECT_CALL(*serialPort, open());
    EXPECT_CALL(*serialPort, write(testing::_, testing::_));
    EXPECT_CALL(*serialPort, close());
    EXPECT_CALL(*serialPort, clear());
    EXPECT_CALL(*serialPort, readAll()).WillOnce(testing::Invoke([&]() {
      return qarray;
    }));

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.sendRequest(request);
    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5 delay
    spy.wait(1000);
    EXPECT_EQ(spy.count(), 0);
    EXPECT_EQ(serialClient.isIdle(), true);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient,
     sendRequestSuccessed_waitingForResponse_readSomethingThenErrorOcurr) {
  declare_app(app);

  {
    Session session;
    createReadCoils<modbus::TransferMode::kRtu>(kServerAddress, kStartAddress,
                                                kQuantity, session);

    auto serialPort = new MockSerialPort();
    serialPort->setupTestForWriteRead();

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &modbus::QModbusClient::requestFinished);

    EXPECT_CALL(*serialPort, open());
    EXPECT_CALL(*serialPort, write(testing::_, testing::_));
    EXPECT_CALL(*serialPort, close());

    QByteArray responseData;
    responseData.append(kServerAddress);

    EXPECT_CALL(*serialPort, readAll())
        .Times(1)
        .WillOnce(testing::Invoke([&]() {
          QTimer::singleShot(10, [&]() { serialPort->error("read error"); });
          return responseData;
        }));

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.sendRequest(session.request);
    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5 delay
    spy.wait(2000);
    /// if some error occured, can not get requestFinished signal
    EXPECT_EQ(spy.count(), 0);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient, sendBrocast_afterSomeDelay_modbusSerialClientInIdle) {
  declare_app(app);

  {
    modbus::Request request = createBrocastRequest();

    auto serialPort = new MockSerialPort();
    serialPort->setupTestForWrite();

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &modbus::QModbusClient::requestFinished);

    EXPECT_CALL(*serialPort, open());
    EXPECT_CALL(*serialPort, write(testing::_, testing::_));
    EXPECT_CALL(*serialPort, close());

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.sendRequest(request);
    /**
     * If it is a broadcast request, then there will be no return results, just
     * a delay for a short period of time, then enter the idle state
     */
    spy.wait(2000);
    /// if some error occured, can not get requestFinished signal
    EXPECT_EQ(spy.count(), 0);
    EXPECT_EQ(serialClient.isIdle(), true);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient, clientIsClosed_sendRequest_requestWillDroped) {
  declare_app(app);

  {
    modbus::Request request = createBrocastRequest();

    auto serialPort = new MockSerialPort();

    modbus::QModbusClient serialClient(serialPort);

    EXPECT_CALL(*serialPort, open()).WillOnce(testing::Invoke([&]() {
      /**
       * No open signal is emitted, so you can simulate an unopened scene
       */
      // serialPort->open();
    }));
    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), false);

    /// send the request
    serialClient.sendRequest(request);
    EXPECT_EQ(serialClient.pendingRequestSize(), 0);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

/**
 * After the disconnection, all pending requests will be deleted. So. if the
 * short-term reconnection, there should be no pending requests
 */
TEST(ModbusSerialClient, connectSuccess_sendFailed_pendingRequestIsZero) {
  declare_app(app);

  {
    auto serialPort = new MockSerialPort();
    modbus::QModbusClient serialClient(serialPort);

    Session session;
    createReadCoils<modbus::TransferMode::kRtu>(kServerAddress, kStartAddress,
                                                kQuantity, session);

    QSignalSpy spy(&serialClient, &modbus::QModbusClient::requestFinished);

    EXPECT_CALL(*serialPort, open()).WillRepeatedly(testing::Invoke([&]() {
      serialPort->opened();
    }));

    EXPECT_CALL(*serialPort, close()).WillRepeatedly(testing::Invoke([&]() {
      serialPort->closed();
    }));
    EXPECT_CALL(*serialPort, write(testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Invoke([&](const char *data, size_t size) {
          serialPort->error("write error, just fot test");
        }));
    serialClient.open();
    serialClient.sendRequest(session.request);
    spy.wait(10000);
    EXPECT_EQ(spy.count(), 0);
    EXPECT_EQ(serialClient.pendingRequestSize(), 0);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient, connect_connectFailed_reconnectSuccess) {
  declare_app(app);

  {
    auto serialPort = new MockSerialPort();
    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &modbus::QModbusClient::clientOpened);

    /**
     * The first time we open, our simulation failed. The second time, we
     * simulated successfully
     */
    EXPECT_CALL(*serialPort, open())
        .Times(2)
        .WillOnce(testing::Invoke([&]() {
          serialPort->error("connect failed");
          return;
        }))
        .WillOnce(testing::Invoke([&]() { serialPort->opened(); }));
    EXPECT_CALL(*serialPort, close())
        .Times(1)
        .WillRepeatedly(testing::Invoke([&]() { serialPort->closed(); }));

    /**
     * if open failed, retry 4 times, interval 2s
     */
    serialClient.setOpenRetryTimes(4, 2000);
    serialClient.open();
    spy.wait(10000);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(serialClient.isOpened(), true);
    EXPECT_EQ(serialClient.isIdle(), true);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient, connectRetryTimesIs4_connectSucces_closeSuccess) {
  declare_app(app);

  {
    auto serialPort = new MockSerialPort();
    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &modbus::QModbusClient::clientOpened);

    /**
     * The first time we open, our simulation failed. The second time, we
     * simulated successfully
     */
    EXPECT_CALL(*serialPort, open())
        .Times(2)
        .WillOnce(testing::Invoke([&]() {
          serialPort->error("connect failed");
          return;
        }))
        .WillOnce(testing::Invoke([&]() { serialPort->opened(); }));
    EXPECT_CALL(*serialPort, close())
        .Times(1)
        .WillRepeatedly(testing::Invoke([&]() { serialPort->closed(); }));

    /**
     * if open failed, retry 4 times, interval 2s
     */
    serialClient.setOpenRetryTimes(4, 2000);
    serialClient.open();
    spy.wait(10000);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(serialClient.isOpened(), true);
    EXPECT_EQ(serialClient.isIdle(), true);

    QSignalSpy spyClose(&serialClient, &modbus::QModbusClient::clientClosed);
    serialClient.close();
    spyClose.wait(1000);
    EXPECT_EQ(spyClose.count(), 1);
    EXPECT_EQ(serialClient.isClosed(), true);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient, sendSingleBitAccess_readCoil_responseIsSuccess) {
  declare_app(app);

  {
    modbus::Request request = createSingleBitAccessRequest();

    auto serialPort = new MockSerialPort();
    serialPort->setupTestForWriteRead();

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &modbus::QModbusClient::requestFinished);

    EXPECT_CALL(*serialPort, open());
    EXPECT_CALL(*serialPort, write(testing::_, testing::_));
    EXPECT_CALL(*serialPort, close());

    modbus::ByteArray responseWithoutCrc = {kServerAddress,
                                            modbus::FunctionCode::kReadCoils,
                                            0x01, 0x05 /*b 0000 0101*/};

    modbus::ByteArray responseWithCrc =
        modbus::tool::appendCrc(responseWithoutCrc);

    QByteArray qarray((const char *)responseWithCrc.data(),
                      responseWithCrc.size());

    EXPECT_CALL(*serialPort, readAll())
        .Times(1)
        .WillOnce(testing::Invoke([&]() { return qarray; }));

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
    request = qvariant_cast<modbus::Request>(arguments.at(0));
    modbus::Response response =
        qvariant_cast<modbus::Response>(arguments.at(1));

    EXPECT_EQ(modbus::Error::kNoError, response.error());
    EXPECT_EQ(false, response.isException());
    auto access =
        modbus::any::any_cast<modbus::SingleBitAccess>(request.userData());
    access.unmarshalReadResponse(response.data());
    EXPECT_EQ(access.value(kStartAddress), modbus::BitValue::kOn);
    EXPECT_EQ(access.value(kStartAddress + 1), modbus::BitValue::kOff);
    EXPECT_EQ(access.value(kStartAddress + 2), modbus::BitValue::kOn);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient,
     sendSingleBitAccess_sendMultipleRequest_responseIsSuccess) {
  declare_app(app);

  {
    modbus::Request request = createSingleBitAccessRequest();

    auto serialPort = new MockSerialPort();

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &modbus::QModbusClient::requestFinished);

    configMockSerialPortWithReponse(
        serialPort,
        modbus::ByteArray{kServerAddress, modbus::FunctionCode::kReadCoils,
                          0x02, 0x01, 0x05 /*b 0000 0101*/});

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    for (int i = 0; i < 5; i++) {
      QTimer::singleShot(10, [&]() { serialClient.sendRequest(request); });
    }
    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(5000);
    EXPECT_EQ(spy.count(), 5);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient, readRegisters_success) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient,
                   &modbus::QModbusClient::readRegistersFinished);

    configMockSerialPortWithReponse(
        serialPort, modbus::ByteArray{kServerAddress, 0x03, 0x08, 0x00, 0x01,
                                      0x00, 0x02, 0x00, 0x03, 0x00, 0x04});

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    modbus::SixteenBitAccess access;

    serialClient.readRegisters(kServerAddress, modbus::FunctionCode(0x03),
                               modbus::Address(0x00), modbus::Quantity(0x04));

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(5000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();

    modbus::Error error = qvariant_cast<modbus::Error>(arguments.at(5));
    EXPECT_EQ(error, modbus::Error::kNoError);

    QVector<modbus::SixteenBitValue> valueList =
        qvariant_cast<QVector<modbus::SixteenBitValue>>(arguments.at(4));
    EXPECT_EQ(valueList.size(), 4);
    EXPECT_EQ(valueList[0].toUint16(), 0x01);
    EXPECT_EQ(valueList[1].toUint16(), 0x02);
    EXPECT_EQ(valueList[2].toUint16(), 0x03);
    EXPECT_EQ(valueList[3].toUint16(), 0x04);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient, readRegisters_failed) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient,
                   &modbus::QModbusClient::readRegistersFinished);

    configMockSerialPortWithReponse(
        serialPort,
        modbus::ByteArray{kServerAddress, 0x03 | 0x80,
                          uint8_t(modbus::Error::kSlaveDeviceBusy)});

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    modbus::SixteenBitAccess access;

    serialClient.readRegisters(kServerAddress, modbus::FunctionCode(0x03),
                               modbus::Address(0x00), modbus::Quantity(0x04));

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(5000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();

    modbus::Error error = qvariant_cast<modbus::Error>(arguments.at(5));
    EXPECT_EQ(error, modbus::Error::kSlaveDeviceBusy);

    QVector<modbus::SixteenBitValue> valueList =
        qvariant_cast<QVector<modbus::SixteenBitValue>>(arguments.at(4));
    EXPECT_EQ(valueList.size(), 0);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusClient, writeSingleRegister_Success) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient,
                   &modbus::QModbusClient::writeSingleRegisterFinished);

    configMockSerialPortWithReponse(
        serialPort,
        modbus::ByteArray{kServerAddress,
                          modbus::FunctionCode::kWriteSingleRegister, 0x00,
                          0x05, 0x00, 0x01});

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.writeSingleRegister(kServerAddress, modbus::Address(0x05),
                                     modbus::SixteenBitValue(0x00, 0x01));

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(1000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    modbus::Address address = qvariant_cast<int>(arguments.at(1));
    EXPECT_EQ(address, 0x05);
    modbus::Error error = qvariant_cast<modbus::Error>(arguments.at(2));
    EXPECT_EQ(error, modbus::Error::kNoError);
  }

  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusClient, writeSingleRegister_Failed) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient,
                   &modbus::QModbusClient::writeSingleRegisterFinished);

    configMockSerialPortWithReponse(
        serialPort,
        modbus::ByteArray{kServerAddress,
                          modbus::FunctionCode::kWriteSingleRegister |
                              modbus::Pdu::kExceptionByte,
                          (uint8_t)modbus::Error::kIllegalFunctionCode});

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.writeSingleRegister(kServerAddress, modbus::Address(0x05),
                                     modbus::SixteenBitValue(0x00, 0x01));

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(1000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    modbus::Address address = qvariant_cast<int>(arguments.at(1));
    EXPECT_EQ(address, 0x05);
    modbus::Error error = qvariant_cast<modbus::Error>(arguments.at(2));
    EXPECT_EQ(error, modbus::Error::kIllegalFunctionCode);
  }

  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusClient, writeMultipleRegisters_success) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient,
                   &modbus::QModbusClient::writeMultipleRegistersFinished);

    configMockSerialPortWithReponse(
        serialPort,
        modbus::ByteArray{kServerAddress,
                          modbus::FunctionCode::kWriteMultipleRegisters, 0x00,
                          0x05, 0x00, 0x03});
    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    QVector<modbus::SixteenBitValue> valueList;
    valueList.push_back(modbus::SixteenBitValue(0x00, 0x01));
    valueList.push_back(modbus::SixteenBitValue(0x00, 0x02));
    valueList.push_back(modbus::SixteenBitValue(0x00, 0x03));
    serialClient.writeMultipleRegisters(kServerAddress, modbus::Address(0x05),
                                        valueList);

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(1000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    modbus::Address address = qvariant_cast<int>(arguments.at(1));
    EXPECT_EQ(address, 0x05);
    modbus::Error error = qvariant_cast<modbus::Error>(arguments.at(2));
    EXPECT_EQ(error, modbus::Error::kNoError);
  }

  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusClient, writeMultipleRegisters_failed) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient,
                   &modbus::QModbusClient::writeMultipleRegistersFinished);

    configMockSerialPortWithReponse(
        serialPort,
        modbus::ByteArray{kServerAddress,
                          modbus::FunctionCode::kWriteMultipleRegisters | 0x80,
                          uint8_t(modbus::Error::kSlaveDeviceBusy)});
    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    QVector<modbus::SixteenBitValue> valueList;
    valueList.push_back(modbus::SixteenBitValue(0x00, 0x01));
    valueList.push_back(modbus::SixteenBitValue(0x00, 0x02));
    valueList.push_back(modbus::SixteenBitValue(0x00, 0x03));
    serialClient.writeMultipleRegisters(kServerAddress, modbus::Address(0x05),
                                        valueList);

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(1000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    modbus::Address address = qvariant_cast<int>(arguments.at(1));
    EXPECT_EQ(address, 0x05);
    modbus::Error error = qvariant_cast<modbus::Error>(arguments.at(2));
    EXPECT_EQ(error, modbus::Error::kSlaveDeviceBusy);
  }

  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusClient, readSingleBits_success) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient,
                   &modbus::QModbusClient::readSingleBitsFinished);

    configMockSerialPortWithReponse(
        serialPort, modbus::ByteArray{kServerAddress,
                                      modbus::FunctionCode::kReadInputDiscrete,
                                      0x01, 0x05 /*b 101*/});
    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.readSingleBits(kServerAddress,
                                modbus::FunctionCode::kReadInputDiscrete,
                                modbus::Address(0x05), modbus::Quantity(3));

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(1000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    modbus::ServerAddress serverAddress =
        qvariant_cast<modbus::ServerAddress>(arguments.at(0));
    EXPECT_EQ(serverAddress, kServerAddress);

    modbus::Address address = qvariant_cast<int>(arguments.at(2));
    EXPECT_EQ(address, 0x05);

    QVector<modbus::BitValue> valueList =
        qvariant_cast<QVector<modbus::BitValue>>(arguments.at(3));
    EXPECT_THAT(valueList, testing::ElementsAre(modbus::BitValue::kOn,
                                                modbus::BitValue::kOff,
                                                modbus::BitValue::kOn));

    modbus::Error error = qvariant_cast<modbus::Error>(arguments.at(4));
    EXPECT_EQ(error, modbus::Error::kNoError);
  }

  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusClient, readSingleBits_failed) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient,
                   &modbus::QModbusClient::readSingleBitsFinished);

    configMockSerialPortWithReponse(
        serialPort,
        modbus::ByteArray{
            kServerAddress,
            uint8_t(modbus::FunctionCode::kReadInputDiscrete | 0x80),
            uint8_t(modbus::Error::kIllegalDataAddress)});
    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.readSingleBits(kServerAddress,
                                modbus::FunctionCode::kReadInputDiscrete,
                                modbus::Address(0x05), modbus::Quantity(3));

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(1000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    modbus::ServerAddress serverAddress =
        qvariant_cast<modbus::ServerAddress>(arguments.at(0));
    EXPECT_EQ(serverAddress, kServerAddress);

    modbus::Address address = qvariant_cast<int>(arguments.at(2));
    EXPECT_EQ(address, 0x05);

    QVector<modbus::BitValue> valueList =
        qvariant_cast<QVector<modbus::BitValue>>(arguments.at(3));
    EXPECT_EQ(valueList.size(), 0);

    modbus::Error error = qvariant_cast<modbus::Error>(arguments.at(4));
    EXPECT_EQ(error, modbus::Error::kIllegalDataAddress);
  }

  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusClient, writeSingleCoil_success) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient,
                   &modbus::QModbusClient::writeSingleCoilFinished);

    configMockSerialPortWithReponse(
        serialPort,
        modbus::ByteArray{kServerAddress,
                          uint8_t(modbus::FunctionCode::kWriteSingleCoil), 0x00,
                          0x05, 0xff, 0x00});
    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.writeSingleCoil(kServerAddress, modbus::Address(0x05),
                                 modbus::BitValue::kOn);

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(1000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    modbus::ServerAddress serverAddress =
        qvariant_cast<modbus::ServerAddress>(arguments.at(0));
    EXPECT_EQ(serverAddress, kServerAddress);

    modbus::Address address = qvariant_cast<int>(arguments.at(1));
    EXPECT_EQ(address, 0x05);

    modbus::Error error = qvariant_cast<modbus::Error>(arguments.at(2));
    EXPECT_EQ(error, modbus::Error::kNoError);
  }

  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusClient, writeSingleCoil_failed) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient,
                   &modbus::QModbusClient::writeSingleCoilFinished);

    configMockSerialPortWithReponse(
        serialPort, modbus::ByteArray{
                        kServerAddress,
                        uint8_t(modbus::FunctionCode::kWriteSingleCoil | 0x80),
                        uint8_t(modbus::Error::kSlaveDeviceBusy)});
    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.writeSingleCoil(kServerAddress, modbus::Address(0x05),
                                 modbus::BitValue::kOff);

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(1000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    modbus::ServerAddress serverAddress =
        qvariant_cast<modbus::ServerAddress>(arguments.at(0));
    EXPECT_EQ(serverAddress, kServerAddress);

    modbus::Address address = qvariant_cast<int>(arguments.at(1));
    EXPECT_EQ(address, 0x05);

    modbus::Error error = qvariant_cast<modbus::Error>(arguments.at(2));
    EXPECT_EQ(error, modbus::Error::kSlaveDeviceBusy);
  }

  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusClient, writeMultipleCoils_success) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient,
                   &modbus::QModbusClient::writeMultipleCoilsFinished);

    configMockSerialPortWithReponse(
        serialPort,
        modbus::ByteArray{kServerAddress,
                          uint8_t(modbus::FunctionCode::kWriteMultipleCoils),
                          0x00, 0x05, 0x00, 0x09});
    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    QVector<modbus::BitValue> valueList;
    valueList.push_back(modbus::BitValue::kOn);
    valueList.push_back(modbus::BitValue::kOff);
    valueList.push_back(modbus::BitValue::kOn);
    valueList.push_back(modbus::BitValue::kOff);
    valueList.push_back(modbus::BitValue::kOn);
    valueList.push_back(modbus::BitValue::kOff);
    valueList.push_back(modbus::BitValue::kOn);
    valueList.push_back(modbus::BitValue::kOff);
    valueList.push_back(modbus::BitValue::kOn);
    serialClient.writeMultipleCoils(kServerAddress, modbus::Address(0x05),
                                    valueList);

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(1000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    modbus::ServerAddress serverAddress =
        qvariant_cast<modbus::ServerAddress>(arguments.at(0));
    EXPECT_EQ(serverAddress, kServerAddress);

    modbus::Address address = qvariant_cast<int>(arguments.at(1));
    EXPECT_EQ(address, 0x05);

    modbus::Error error = qvariant_cast<modbus::Error>(arguments.at(2));
    EXPECT_EQ(error, modbus::Error::kNoError);
  }

  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusClient, writeMultipleCoils_failed) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient,
                   &modbus::QModbusClient::writeMultipleCoilsFinished);

    configMockSerialPortWithReponse(
        serialPort,
        modbus::ByteArray{
            kServerAddress,
            uint8_t(modbus::FunctionCode::kWriteMultipleCoils | 0x80),
            uint8_t(modbus::Error::kSlaveDeviceBusy)});
    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    QVector<modbus::BitValue> valueList;
    valueList.push_back(modbus::BitValue::kOn);
    valueList.push_back(modbus::BitValue::kOff);
    valueList.push_back(modbus::BitValue::kOn);
    valueList.push_back(modbus::BitValue::kOff);
    valueList.push_back(modbus::BitValue::kOn);
    valueList.push_back(modbus::BitValue::kOff);
    valueList.push_back(modbus::BitValue::kOn);
    valueList.push_back(modbus::BitValue::kOff);
    valueList.push_back(modbus::BitValue::kOn);
    serialClient.writeMultipleCoils(kServerAddress, modbus::Address(0x05),
                                    valueList);

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(1000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    modbus::ServerAddress serverAddress =
        qvariant_cast<modbus::ServerAddress>(arguments.at(0));
    EXPECT_EQ(serverAddress, kServerAddress);

    modbus::Address address = qvariant_cast<int>(arguments.at(1));
    EXPECT_EQ(address, 0x05);

    modbus::Error error = qvariant_cast<modbus::Error>(arguments.at(2));
    EXPECT_EQ(error, modbus::Error::kSlaveDeviceBusy);
  }

  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusClient, readWriteMultipleRegisters_success) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();

    modbus::QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient,
                   &modbus::QModbusClient::readWriteMultipleRegistersFinished);

    configMockSerialPortWithReponse(
        serialPort,
        modbus::ByteArray{
            kServerAddress,
            uint8_t(modbus::FunctionCode::kReadWriteMultipleRegisters), 0x06,
            0x00, 0x01, 0x00, 0x02, 0x00, 0x03});
    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    QVector<modbus::SixteenBitValue> valueList;

    valueList.push_back(modbus::SixteenBitValue(0x00, 0x0a));
    valueList.push_back(modbus::SixteenBitValue(0x00, 0x0b));
    valueList.push_back(modbus::SixteenBitValue(0x00, 0x0c));
    valueList.push_back(modbus::SixteenBitValue(0x00, 0x0d));

    serialClient.readWriteMultipleRegisters(
        kServerAddress, modbus::Address(0x05), modbus::Quantity(0x03),
        modbus::Address(0x04), valueList);

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(1000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    modbus::ServerAddress serverAddress =
        qvariant_cast<modbus::ServerAddress>(arguments.at(0));
    EXPECT_EQ(serverAddress, kServerAddress);

    modbus::Address readStartAddress = qvariant_cast<int>(arguments.at(1));
    EXPECT_EQ(readStartAddress, 0x05);

    valueList =
        qvariant_cast<QVector<modbus::SixteenBitValue>>(arguments.at(2));
    EXPECT_EQ(valueList.size(), 3);
    EXPECT_EQ(valueList[0].toUint16(), 0x01);
    EXPECT_EQ(valueList[1].toUint16(), 0x02);
    EXPECT_EQ(valueList[2].toUint16(), 0x03);

    modbus::Error error = qvariant_cast<modbus::Error>(arguments.at(3));
    EXPECT_EQ(error, modbus::Error::kNoError);
  }

  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusClient, frame_diagnostics) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();

    modbus::QModbusClient serialClient(serialPort);
    serialClient.setRetryTimes(1);
    serialClient.setTimeout(300);
    serialClient.enableDiagnosis(true);

    QSignalSpy spy(&serialClient,
                   &modbus::QModbusClient::readRegistersFinished);

    modbus::FunctionCode funcode = modbus::FunctionCode::kReadHoldingRegisters;
    modbus::ByteArray emptyResponse = {};
    modbus::ByteArray successReponse = {kServerAddress, uint8_t(funcode), 0x02,
                                        0x00, 0x01};
    modbus::ByteArray failedReponse = {
        kServerAddress, uint8_t(funcode | 0x80),
        uint8_t(modbus::Error::kSlaveDeviceBusy)};

    configMockSerialPortWithReponse(serialPort, {});
    EXPECT_CALL(*serialPort, readAll())
        .WillOnce(testing::Invoke(
            [emptyResponse]() { return buildResponseArray(emptyResponse); }))
        .WillOnce(testing::Invoke(
            [successReponse]() { return buildResponseArray(successReponse); }))
        .WillOnce(testing::Invoke(
            [failedReponse]() { return buildResponseArray(failedReponse); }));
    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.readRegisters(kServerAddress, funcode, modbus::Address(0x05),
                               modbus::Quantity(0x01));
    serialClient.readRegisters(kServerAddress, funcode, modbus::Address(0x05),
                               modbus::Quantity(0x01));

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(5000);
    EXPECT_EQ(spy.count(), 2);
    modbus::RuntimeDiagnosis diagnosis = serialClient.runtimeDiagnosis();

    /// timout(1) + + retry-success(1) + failed(1)
    EXPECT_EQ(diagnosis.totalFrameNumbers(), 3);

    EXPECT_EQ(diagnosis.successedFrameNumbers(), 1);
    EXPECT_EQ(diagnosis.failedFrameNumbers(), 2);
    EXPECT_EQ(diagnosis.servers().size(), 1);

    auto servers = diagnosis.servers();
    EXPECT_NE(servers.find(kServerAddress), servers.end());
    const auto &server = servers[kServerAddress];
    EXPECT_EQ(server.errorRecords().size(), 2);
    EXPECT_EQ(server.errorRecords()[0].error(), modbus::Error::kTimeout);
    EXPECT_EQ(server.errorRecords()[1].error(),
              modbus::Error::kSlaveDeviceBusy);
  }

  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

template <modbus::TransferMode mode>
static void createReadCoils(modbus::ServerAddress serverAddress,
                            modbus::Address startAddress,
                            modbus::Quantity quantity, Session &session) {
  modbus::SingleBitAccess access;

  access.setStartAddress(kStartAddress);
  access.setQuantity(kQuantity);

  for (int i = 0; i < access.quantity(); i++) {
    access.setValue(access.startAddress() + i, i % 2 == 0
                                                   ? modbus::BitValue::kOn
                                                   : modbus::BitValue::kOff);
  }

  session.request.setServerAddress(kServerAddress);
  session.request.setFunctionCode(modbus::FunctionCode::kReadCoils);
  session.request.setDataChecker(MockReadCoilsDataChecker::newDataChecker());
  session.request.setData(access.marshalReadRequest());
  session.request.setUserData(access);

  session.response.setServerAddress(kServerAddress);
  session.response.setFunctionCode(modbus::FunctionCode::kReadCoils);
  session.response.setData(access.marshalReadResponse());
  session.response.setDataChecker(MockReadCoilsDataChecker::newDataChecker());

  modbus::ByteArray aduWithoutCrcRequest =
      session.request.marshalAduWithoutCrc();
  modbus::ByteArray aduWithoutCrcResponse =
      session.response.marshalAduWithoutCrc();
  if (mode == modbus::TransferMode::kRtu) {
    session.requestRaw = modbus::marshalRtuFrame(aduWithoutCrcRequest);
    session.responseRaw = modbus::marshalRtuFrame(aduWithoutCrcResponse);
  } else if (mode == modbus::TransferMode::kAscii) {
    session.requestRaw = modbus::marshalAsciiFrame(aduWithoutCrcRequest);
    session.responseRaw = modbus::marshalAsciiFrame(aduWithoutCrcResponse);
  }
}

static modbus::Request createSingleBitAccessRequest() {
  modbus::SingleBitAccess access;

  access.setStartAddress(kStartAddress);
  access.setQuantity(kQuantity);

  modbus::Request request;

  request.setServerAddress(kServerAddress);
  request.setFunctionCode(modbus::FunctionCode::kReadCoils);
  request.setDataChecker(MockReadCoilsDataChecker::newDataChecker());
  request.setData(access.marshalReadRequest());
  request.setUserData(access);
  return request;
}

static modbus::Request createBrocastRequest() {
  modbus::SingleBitAccess access;

  access.setStartAddress(kStartAddress);
  access.setQuantity(kQuantity);

  modbus::Request request;

  request.setServerAddress(modbus::Adu::kBrocastAddress);
  request.setFunctionCode(modbus::FunctionCode::kReadCoils);
  request.setDataChecker(MockReadCoilsDataChecker::newDataChecker());
  request.setData(access.marshalReadRequest());
  request.setUserData(access);
  return request;
}

/**
 *this will append crc to expectResponse
 */
static void configMockSerialPortWithReponse(MockSerialPort *mocker,
                                            const modbus::ByteArray &response) {
  EXPECT_CALL(*mocker, open()).WillRepeatedly(testing::Invoke([mocker]() {
    mocker->opened();
  }));
  EXPECT_CALL(*mocker, write(testing::_, testing::_))
      .WillRepeatedly(testing::Invoke([mocker](const char *data, size_t size) {
        mocker->bytesWritten(size);
        QTimer::singleShot(0, [mocker]() { mocker->readyRead(); });
      }));
  EXPECT_CALL(*mocker, close()).WillRepeatedly(testing::Invoke([mocker]() {
    mocker->closed();
  }));
  EXPECT_CALL(*mocker, readAll()).WillRepeatedly(testing::Invoke([response]() {
    return buildResponseArray(response);
  }));
}

static QByteArray buildResponseArray(const modbus::ByteArray &response) {
  modbus::ByteArray responseWithCrc = modbus::tool::appendCrc(response);
  QByteArray qarray((const char *)responseWithCrc.data(),
                    responseWithCrc.size());
  return qarray;
}
