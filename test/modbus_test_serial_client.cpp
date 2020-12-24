#include "modbus_test_mocker.h"
#include "tools/modbus_client_p.h"
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <modbus/base/single_bit_access.h>
#include <modbus/base/sixteen_bit_access.h>
#include <modbus_frame.h>

using namespace testing;
using namespace modbus;

struct Session {
  std::unique_ptr<Request> request;
  ByteArray requestRaw;
  Response response;
  ByteArray responseRaw;
};

#define declare_app(name)                                                      \
  int argc = 1;                                                                \
  char *argv[] = {(char *)"test"};                                             \
  QCoreApplication name(argc, argv);

static const Address kStartAddress = 10;
static const Quantity kQuantity = 3;
static const ServerAddress kServerAddress = 1;
static const ServerAddress kBadServerAddress = 0x11;
static std::unique_ptr<Request> createSingleBitAccessRequest();
static std::unique_ptr<Request> createBrocastRequest();
template <TransferMode mode>
static void createReadCoils(ServerAddress serverAddress, Address startAddress,
                            Quantity quantity, Session &session);

TEST(ModbusClient, ClientConstruct_defaultIsClosed) {
  auto serialPort = new MockSerialPort();
  QModbusClient mockSerialClient(serialPort);
  EXPECT_EQ(mockSerialClient.transferMode(), TransferMode::kRtu);
  mockSerialClient.setTransferMode(TransferMode::kRtu);
  EXPECT_EQ(mockSerialClient.transferMode(), TransferMode::kRtu);
  EXPECT_EQ(mockSerialClient.isClosed(), true);
}

TEST(ModbusSerialClient, clientIsClosed_openDevice_clientIsOpened) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();
    QModbusClient serialClient(serialPort);
    serialClient.setTransferMode(TransferMode::kRtu);

    QSignalSpy spy(&serialClient, &QModbusClient::clientOpened);

    serialClient.open();
    spy.wait(300);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(serialClient.isOpened(), true);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient, clientIsClosed_openSerial_retry4TimesFailed) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();
    QModbusClient serialClient(serialPort);

    EXPECT_CALL(*serialPort, open).WillRepeatedly(Invoke([&]() {
      emit serialPort->error("open serial failed");
    }));

    QSignalSpy spy(&serialClient, &QModbusClient::clientOpened);

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
  {
    QModbusClient serialClient(serialPort);
    QSignalSpy spyOpen(&serialClient, &QModbusClient::clientOpened);
    QSignalSpy spyClose(&serialClient, &QModbusClient::clientClosed);

    // make sure the client is opened
    serialClient.open();
    spyOpen.wait(300);
    EXPECT_EQ(spyOpen.count(), 1);
    EXPECT_EQ(serialClient.isOpened(), true);

    // now close the client
    serialClient.close();
    spyClose.wait(300);
    EXPECT_EQ(spyClose.count(), 1);

    EXPECT_EQ(serialClient.isClosed(), true);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient, clientOpened_sendRequest_clientWriteFailed) {
  declare_app(app);

  Session session;
  createReadCoils<TransferMode::kRtu>(kServerAddress, kStartAddress, kQuantity,
                                      session);

  {
    auto serialPort = new MockSerialPort();
    QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &QModbusClient::errorOccur);

    EXPECT_CALL(*serialPort, write(_, _))
        .WillRepeatedly(Invoke([&](const char *data, size_t size) {
          emit serialPort->error("write error");
        }));

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);
    std::unique_ptr<Request> req(new Request);
    *req = *session.request;
    serialClient.sendRequest(req);
    req.reset(new Request);
    *req = *session.request;
    serialClient.sendRequest(req);
    req.reset(new Request);
    *req = *session.request;
    serialClient.sendRequest(req);
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
  createReadCoils<TransferMode::kRtu>(kServerAddress, kStartAddress, kQuantity,
                                      session);
  {
    auto serialPort = new MockSerialPort();
    QModbusClient serialClient(serialPort);
    ByteArray sentData;

    ON_CALL(*serialPort, write)
        .WillByDefault(Invoke([&](const char *data, size_t size) {
          sentData.insert(sentData.end(), data, data + size);
          emit serialPort->bytesWritten(size);
        }));

    QSignalSpy spy(&serialClient, &QModbusClient::requestFinished);

    EXPECT_CALL(*serialPort, write(_, _));

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.sendRequest(session.request);
    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5 delay
    /// because we not mock readAll(),so it will timeout,and so,
    /// we set 3000ms for waiting
    spy.wait(300);
    EXPECT_EQ(sentData, session.requestRaw);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient, setTimeout) {
  declare_app(app);

  {
    auto serialPort = new MockSerialPort();
    QModbusClient serialClient(serialPort);

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
    QModbusClient serialClient(serialPort);

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
    createReadCoils<TransferMode::kRtu>(kServerAddress, kStartAddress,
                                        kQuantity, session);

    auto serialPort = new MockSerialPort();

    QModbusClient serialClient(serialPort);

    serialClient.setRetryTimes(2);
    serialClient.setTimeout(500);
    QSignalSpy spy(&serialClient, &QModbusClient::requestFinished);

    /**
     * we set the retry times 2,so write() will be called twice
     */
    EXPECT_CALL(*serialPort, write(_, _))
        .Times(3)
        .WillRepeatedly(Invoke([&](const char *data, size_t size) {
          serialPort->bytesWritten(size);
        }));

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.sendRequest(session.request);
    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5 delay
    spy.wait(10000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    Response response = qvariant_cast<Response>(arguments.at(1));
    EXPECT_EQ(Error::kTimeout, response.error());
    EXPECT_EQ(2, serialClient.retryTimes());
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient,
     sendRequestSuccessed_waitingForResponse_responseGetSuccessed) {
  declare_app(app);

  {
    auto serialPort = new MockSerialPort();
    QModbusClient serialClient(serialPort);
    QSignalSpy spy(&serialClient, &QModbusClient::requestFinished);

    EXPECT_CALL(*serialPort, write)
        .WillRepeatedly(Invoke([&](const char *data, size_t size) {
          emit serialPort->bytesWritten(size);
          QTimer::singleShot(10, [&]() { emit serialPort->readyRead(); });
        }));
    EXPECT_CALL(*serialPort, readAll())
        .Times(4)
        .WillOnce(Invoke([&]() {
          QTimer::singleShot(10, [&]() { serialPort->readyRead(); });
          return QByteArray("\x01", 1);
        }))
        .WillOnce(Invoke([&]() {
          QTimer::singleShot(10, [&]() { serialPort->readyRead(); });
          return QByteArray("\x01\x01\x05", 3);
        }))
        .WillOnce(Invoke([&]() {
          QTimer::singleShot(10, [&]() { serialPort->readyRead(); });
          return QByteArray("\x91", 1);
        }))
        .WillOnce(Invoke([&]() { return QByteArray("\x8b", 1); }));

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    std::unique_ptr<Request> req(new Request);
    req->setServerAddress(0x01);
    req->setFunctionCode(FunctionCode::kReadCoils);
    req->setData({0x00, 0x0a, 0x00, 0x03});
    req->setDataChecker({bytesRequiredStoreInArrayIndex<0>});
    serialClient.sendRequest(req);
    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5 delay
    spy.wait(200000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    Response response = qvariant_cast<Response>(arguments.at(1));
    EXPECT_EQ(Error::kNoError, response.error());
    EXPECT_EQ(FunctionCode::kReadCoils, response.functionCode());
    EXPECT_EQ(0x01, response.serverAddress());
    EXPECT_EQ(response.data(), ByteArray({0x01, 0x05}));
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient,
     sendRequestSuccessed_waitingForResponse_responseCrcError) {
  declare_app(app);

  {
    Session session;
    createReadCoils<TransferMode::kRtu>(kServerAddress, kStartAddress,
                                        kQuantity, session);
    auto serialPort = new MockSerialPort();

    QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &QModbusClient::requestFinished);

    EXPECT_CALL(*serialPort, write(_, _));

    session.responseRaw[session.responseRaw.size() - 1] = 0x00;
    QByteArray qarray((const char *)session.responseRaw.data(),
                      session.responseRaw.size());

    EXPECT_CALL(*serialPort, readAll()).Times(1).WillOnce(Invoke([&]() {
      return qarray;
    }));

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
    Response response = qvariant_cast<Response>(arguments.at(1));
    EXPECT_EQ(Error::kStorageParityError, response.error());
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient,
     sendRequestSuccessed_waitingForResponse_responseExpection) {
  declare_app(app);

  {
    Session session;
    createReadCoils<TransferMode::kRtu>(kServerAddress, kStartAddress,
                                        kQuantity, session);
    FunctionCode functionCode =
        FunctionCode(session.response.functionCode() | Pdu::kExceptionByte);
    session.response.setFunctionCode(functionCode);
    session.response.setError(Error::kSlaveDeviceBusy);
    session.responseRaw =
        marshalRtuFrame(session.response.marshalAduWithoutCrc());

    auto serialPort = new MockSerialPort();

    QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &QModbusClient::requestFinished);

    EXPECT_CALL(*serialPort, write(_, _));

    ByteArray responseWithoutCrc = {
        kServerAddress,
        /**
         * use FunctionCode::kReadCoils | Pdu::kExceptionByte
         * simulated exception return
         */
        FunctionCode::kReadCoils | Pdu::kExceptionByte,
        static_cast<uint8_t>(Error::kSlaveDeviceBusy)};

    ByteArray responseWithCrc = tool::appendCrc(responseWithoutCrc);
    QByteArray qarray((const char *)responseWithCrc.data(),
                      responseWithCrc.size());

    EXPECT_CALL(*serialPort, readAll()).Times(1).WillOnce(Invoke([&]() {
      return qarray;
    }));

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
    Response response = qvariant_cast<Response>(arguments.at(1));
    EXPECT_EQ(Error::kSlaveDeviceBusy, response.error());
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
    createReadCoils<TransferMode::kRtu>(kServerAddress, kStartAddress,
                                        kQuantity, session);
    FunctionCode functionCode =
        FunctionCode(session.response.functionCode() | Pdu::kExceptionByte);
    session.response.setServerAddress(0x00);
    session.response.setFunctionCode(functionCode);
    session.response.setError(Error::kTimeout);
    session.responseRaw =
        marshalRtuFrame(session.response.marshalAduWithoutCrc());

    auto serialPort = new MockSerialPort();

    QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &QModbusClient::requestFinished);

    EXPECT_CALL(*serialPort, write)
        .WillRepeatedly(Invoke([&](const char *data, size_t size) {
          emit serialPort->bytesWritten(size);
          QTimer::singleShot(10, [&]() { emit serialPort->readyRead(); });
        }));

    QByteArray qarray((const char *)session.responseRaw.data(),
                      session.responseRaw.size());
    EXPECT_CALL(*serialPort, readAll()).Times(1).WillOnce(Invoke([&]() {
      return qarray;
    }));

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
    Response response = qvariant_cast<Response>(arguments.at(1));
    EXPECT_EQ(Error::kTimeout, response.error());
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient, sendBrocast_gotResponse_discardIt) {
  declare_app(app);

  {
    auto serialPort = new MockSerialPort();
    QModbusClient serialClient(serialPort);
    QSignalSpy spy(&serialClient, &QModbusClient::requestFinished);

    EXPECT_CALL(*serialPort, write(_, _));
    EXPECT_CALL(*serialPort, readAll()).WillOnce(Invoke([&]() {
      return QByteArray("\x01\x01\x01\x05\x91\x8b", 6);
    }));

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    auto request = createBrocastRequest();
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
    createReadCoils<TransferMode::kRtu>(kServerAddress, kStartAddress,
                                        kQuantity, session);

    auto serialPort = new MockSerialPort();

    QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &QModbusClient::requestFinished);

    EXPECT_CALL(*serialPort, write)
        .WillRepeatedly(Invoke([&](const char *data, size_t size) {
          emit serialPort->bytesWritten(size);
          QTimer::singleShot(10, [&]() { emit serialPort->readyRead(); });
        }));

    QByteArray responseData;
    responseData.append(kServerAddress);

    EXPECT_CALL(*serialPort, readAll()).Times(1).WillOnce(Invoke([&]() {
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
    auto request = createBrocastRequest();

    auto serialPort = new MockSerialPort();

    QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &QModbusClient::requestFinished);

    EXPECT_CALL(*serialPort, write(_, _))
        .WillRepeatedly(Invoke([&](const char *data, size_t size) {
          emit serialPort->bytesWritten(size);
        }));

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
    auto request = createBrocastRequest();

    auto serialPort = new MockSerialPort();

    QModbusClient serialClient(serialPort);

    EXPECT_CALL(*serialPort, open()).WillOnce(Invoke([&]() {
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
    QModbusClient serialClient(serialPort);

    Session session;
    createReadCoils<TransferMode::kRtu>(kServerAddress, kStartAddress,
                                        kQuantity, session);

    QSignalSpy spy(&serialClient, &QModbusClient::requestFinished);

    EXPECT_CALL(*serialPort, open()).WillRepeatedly(Invoke([&]() {
      serialPort->opened();
    }));

    EXPECT_CALL(*serialPort, close()).WillRepeatedly(Invoke([&]() {
      serialPort->closed();
    }));
    EXPECT_CALL(*serialPort, write(_, _))
        .Times(1)
        .WillOnce(Invoke([&](const char *data, size_t size) {
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
    QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &QModbusClient::clientOpened);

    /**
     * The first time we open, our simulation failed. The second time, we
     * simulated successfully
     */
    EXPECT_CALL(*serialPort, open())
        .Times(2)
        .WillOnce(Invoke([&]() {
          serialPort->error("connect failed");
          return;
        }))
        .WillOnce(Invoke([&]() { serialPort->opened(); }));
    EXPECT_CALL(*serialPort, close()).Times(1).WillRepeatedly(Invoke([&]() {
      serialPort->closed();
    }));

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
    QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &QModbusClient::clientOpened);

    /**
     * The first time we open, our simulation failed. The second time, we
     * simulated successfully
     */
    EXPECT_CALL(*serialPort, open())
        .Times(2)
        .WillOnce(Invoke([&]() {
          serialPort->error("connect failed");
          return;
        }))
        .WillOnce(Invoke([&]() { serialPort->opened(); }));
    EXPECT_CALL(*serialPort, close()).Times(1).WillRepeatedly(Invoke([&]() {
      serialPort->closed();
    }));

    /**
     * if open failed, retry 4 times, interval 2s
     */
    serialClient.setOpenRetryTimes(4, 2000);
    serialClient.open();
    spy.wait(10000);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(serialClient.isOpened(), true);
    EXPECT_EQ(serialClient.isIdle(), true);

    QSignalSpy spyClose(&serialClient, &QModbusClient::clientClosed);
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
    auto request = createSingleBitAccessRequest();

    auto serialPort = new MockSerialPort();

    QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &QModbusClient::requestFinished);

    EXPECT_CALL(*serialPort, write(_, _));

    ByteArray responseWithoutCrc = {kServerAddress, FunctionCode::kReadCoils,
                                    0x01, 0x05 /*b 0000 0101*/};

    ByteArray responseWithCrc = tool::appendCrc(responseWithoutCrc);

    QByteArray qarray((const char *)responseWithCrc.data(),
                      responseWithCrc.size());

    EXPECT_CALL(*serialPort, readAll()).Times(1).WillOnce(Invoke([&]() {
      return qarray;
    }));

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
    auto myrequest = qvariant_cast<Request>(arguments.at(0));
    Response response = qvariant_cast<Response>(arguments.at(1));

    EXPECT_EQ(Error::kNoError, response.error());
    EXPECT_FALSE(response.isException());
    auto access = any::any_cast<SingleBitAccess>(myrequest.userData());
    access.unmarshalReadResponse(response.data());
    EXPECT_EQ(access.value(kStartAddress), BitValue::kOn);
    EXPECT_EQ(access.value(kStartAddress + 1), BitValue::kOff);
    EXPECT_EQ(access.value(kStartAddress + 2), BitValue::kOn);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusSerialClient,
     sendSingleBitAccess_sendMultipleRequest_responseIsSuccess) {
  declare_app(app);

  {
    auto serialPort = new MockSerialPort();
    QModbusClient serialClient(serialPort);
    QSignalSpy spy(&serialClient, &QModbusClient::requestFinished);
    EXPECT_CALL(*serialPort, write).Times(5);
    EXPECT_CALL(*serialPort, readAll()).WillRepeatedly(Invoke([]() {
      return QByteArray("\x01\x01\x02\x01\x05\x78\x6f", 7);
    }));

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    for (int i = 0; i < 5; i++) {
      QTimer::singleShot(10, [&]() {
        std::unique_ptr<Request> request(new Request);
        request->setServerAddress(0x01);
        request->setFunctionCode(FunctionCode::kReadCoils);
        request->setDataChecker({bytesRequiredStoreInArrayIndex<0>});
        request->setData({0x00, 0x0a, 0x00, 0x03});
        serialClient.sendRequest(request);
      });
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

    QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &QModbusClient::readRegistersFinished);

    EXPECT_CALL(*serialPort, write);
    EXPECT_CALL(*serialPort, readAll()).WillRepeatedly(Invoke([]() {
      return QByteArray("\x01\x03\x08\x00\x01\x00\x02\x00\x03\x00\x04\x0d\x14",
                        13);
    }));

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    SixteenBitAccess access;

    serialClient.readRegisters(kServerAddress, FunctionCode(0x03),
                               Address(0x00), Quantity(0x04));

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(5000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();

    Error error = qvariant_cast<Error>(arguments.at(5));
    EXPECT_EQ(error, Error::kNoError);

    auto data = qvariant_cast<ByteArray>(arguments.at(4));
    EXPECT_EQ(data.size(), 8);
    EXPECT_EQ(data[0], 0x00);
    EXPECT_EQ(data[1], 0x01);
    EXPECT_EQ(data[2], 0x00);
    EXPECT_EQ(data[3], 0x02);
    EXPECT_EQ(data[4], 0x00);
    EXPECT_EQ(data[5], 0x03);
    EXPECT_EQ(data[6], 0x00);
    EXPECT_EQ(data[7], 0x04);
  }
  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusClient, writeSingleRegister_Success) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();
    QModbusClient serialClient(serialPort);
    QSignalSpy spy(&serialClient, &QModbusClient::writeSingleRegisterFinished);

    EXPECT_CALL(*serialPort, write);
    EXPECT_CALL(*serialPort, readAll()).WillRepeatedly(Invoke([]() {
      return QByteArray("\x01\x06\x00\x05\x00\x01\x58\x0b", 8);
    }));

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.writeSingleRegister(0x01, Address(0x05),
                                     SixteenBitValue(0x00, 0x01));

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(1000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    Address address = qvariant_cast<int>(arguments.at(1));
    EXPECT_EQ(address, 0x05);
    Error error = qvariant_cast<Error>(arguments.at(2));
    EXPECT_EQ(error, Error::kNoError);
  }

  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusClient, writeMultipleRegisters_success) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();
    QModbusClient serialClient(serialPort);
    QSignalSpy spy(&serialClient,
                   &QModbusClient::writeMultipleRegistersFinished);

    EXPECT_CALL(*serialPort, write);
    EXPECT_CALL(*serialPort, readAll()).WillRepeatedly(Invoke([]() {
      return QByteArray("\x01\x10\x00\x05\x00\x03\x90\x09", 8);
    }));
    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    QVector<SixteenBitValue> valueList;
    valueList.push_back(SixteenBitValue(0x00, 0x01));
    valueList.push_back(SixteenBitValue(0x00, 0x02));
    valueList.push_back(SixteenBitValue(0x00, 0x03));
    serialClient.writeMultipleRegisters(kServerAddress, Address(0x05),
                                        valueList);

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(1000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    Address address = qvariant_cast<int>(arguments.at(1));
    EXPECT_EQ(address, 0x05);
    Error error = qvariant_cast<Error>(arguments.at(2));
    EXPECT_EQ(error, Error::kNoError);
  }

  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusClient, readSingleBits_success) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();
    QModbusClient serialClient(serialPort);
    QSignalSpy spy(&serialClient, &QModbusClient::readSingleBitsFinished);

    EXPECT_CALL(*serialPort, write);
    EXPECT_CALL(*serialPort, readAll()).WillRepeatedly(Invoke([]() {
      return QByteArray("\x01\x02\x01\x05\x61\x8b", 6);
    }));
    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.readSingleBits(kServerAddress,
                                FunctionCode::kReadInputDiscrete, Address(0x05),
                                Quantity(3));

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(1000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    ServerAddress serverAddress = qvariant_cast<ServerAddress>(arguments.at(0));
    EXPECT_EQ(serverAddress, kServerAddress);

    Address address = qvariant_cast<int>(arguments.at(2));
    EXPECT_EQ(address, 0x05);

    Quantity quantity = qvariant_cast<Quantity>(arguments.at(3));
    EXPECT_EQ(quantity, 3);

    QVector<BitValue> valueList =
        qvariant_cast<QVector<BitValue>>(arguments.at(4));
    EXPECT_THAT(valueList,
                ElementsAre(BitValue::kOn, BitValue::kOff, BitValue::kOn));

    Error error = qvariant_cast<Error>(arguments.at(5));
    EXPECT_EQ(error, Error::kNoError);
  }

  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusClient, writeSingleCoil_success) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();
    QModbusClient serialClient(serialPort);
    QSignalSpy spy(&serialClient, &QModbusClient::writeSingleCoilFinished);

    EXPECT_CALL(*serialPort, write);
    EXPECT_CALL(*serialPort, readAll()).WillRepeatedly(Invoke([]() {
      return QByteArray("\x01\x05\x00\x05\xff\x00\x9c\x3b", 8);
    }));
    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.writeSingleCoil(kServerAddress, Address(0x05), BitValue::kOn);

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(1000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    ServerAddress serverAddress = qvariant_cast<ServerAddress>(arguments.at(0));
    EXPECT_EQ(serverAddress, kServerAddress);

    Address address = qvariant_cast<int>(arguments.at(1));
    EXPECT_EQ(address, 0x05);

    Error error = qvariant_cast<Error>(arguments.at(2));
    EXPECT_EQ(error, Error::kNoError);
  }

  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusClient, writeMultipleCoils_success) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();

    QModbusClient serialClient(serialPort);

    QSignalSpy spy(&serialClient, &QModbusClient::writeMultipleCoilsFinished);

    EXPECT_CALL(*serialPort, write);
    EXPECT_CALL(*serialPort, readAll()).WillRepeatedly(Invoke([]() {
      return QByteArray("\x01\x0f\x00\x05\x00\x09\x85\xcc", 8);
    }));

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    QVector<BitValue> valueList;
    valueList.push_back(BitValue::kOn);
    valueList.push_back(BitValue::kOff);
    valueList.push_back(BitValue::kOn);
    valueList.push_back(BitValue::kOff);
    valueList.push_back(BitValue::kOn);
    valueList.push_back(BitValue::kOff);
    valueList.push_back(BitValue::kOn);
    valueList.push_back(BitValue::kOff);
    valueList.push_back(BitValue::kOn);
    serialClient.writeMultipleCoils(kServerAddress, Address(0x05), valueList);

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(1000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    ServerAddress serverAddress = qvariant_cast<ServerAddress>(arguments.at(0));
    EXPECT_EQ(serverAddress, kServerAddress);

    Address address = qvariant_cast<int>(arguments.at(1));
    EXPECT_EQ(address, 0x05);

    Error error = qvariant_cast<Error>(arguments.at(2));
    EXPECT_EQ(error, Error::kNoError);
  }

  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusClient, writeMultipleCoils_failed) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();
    QModbusClient serialClient(serialPort);
    QSignalSpy spy(&serialClient, &QModbusClient::writeMultipleCoilsFinished);

    EXPECT_CALL(*serialPort, write);
    EXPECT_CALL(*serialPort, readAll()).WillRepeatedly(Invoke([]() {
      return QByteArray("\x01\x8f\x06\xc4\x32", 5);
    }));
    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    QVector<BitValue> valueList;
    valueList.push_back(BitValue::kOn);
    valueList.push_back(BitValue::kOff);
    valueList.push_back(BitValue::kOn);
    valueList.push_back(BitValue::kOff);
    valueList.push_back(BitValue::kOn);
    valueList.push_back(BitValue::kOff);
    valueList.push_back(BitValue::kOn);
    valueList.push_back(BitValue::kOff);
    valueList.push_back(BitValue::kOn);
    serialClient.writeMultipleCoils(kServerAddress, Address(0x05), valueList);

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(1000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    ServerAddress serverAddress = qvariant_cast<ServerAddress>(arguments.at(0));
    EXPECT_EQ(serverAddress, kServerAddress);

    Address address = qvariant_cast<int>(arguments.at(1));
    EXPECT_EQ(address, 0x05);

    Error error = qvariant_cast<Error>(arguments.at(2));
    EXPECT_EQ(error, Error::kSlaveDeviceBusy);
  }

  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusClient, readWriteMultipleRegisters_success) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();
    QModbusClient serialClient(serialPort);
    QSignalSpy spy(&serialClient,
                   &QModbusClient::readWriteMultipleRegistersFinished);

    EXPECT_CALL(*serialPort, write);
    EXPECT_CALL(*serialPort, readAll()).WillRepeatedly(Invoke([]() {
      return QByteArray("\x01\x17\x06\x00\x01\x00\x02\x00\x03\xfd\x8b", 11);
    }));

    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    QVector<SixteenBitValue> valueList;
    valueList.push_back(SixteenBitValue(0x00, 0x0a));
    valueList.push_back(SixteenBitValue(0x00, 0x0b));
    valueList.push_back(SixteenBitValue(0x00, 0x0c));
    valueList.push_back(SixteenBitValue(0x00, 0x0d));

    serialClient.readWriteMultipleRegisters(0x01, Address(0x05), Quantity(0x03),
                                            Address(0x04), valueList);

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(1000);
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    ServerAddress serverAddress = qvariant_cast<ServerAddress>(arguments.at(0));
    EXPECT_EQ(serverAddress, kServerAddress);

    Address readStartAddress = qvariant_cast<int>(arguments.at(1));
    EXPECT_EQ(readStartAddress, 0x05);

    valueList = qvariant_cast<QVector<SixteenBitValue>>(arguments.at(2));
    EXPECT_EQ(valueList.size(), 3);
    EXPECT_EQ(valueList[0].toUint16(), 0x01);
    EXPECT_EQ(valueList[1].toUint16(), 0x02);
    EXPECT_EQ(valueList[2].toUint16(), 0x03);

    Error error = qvariant_cast<Error>(arguments.at(3));
    EXPECT_EQ(error, Error::kNoError);
  }

  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

TEST(ModbusClient, frame_diagnostics) {
  declare_app(app);
  {
    auto serialPort = new MockSerialPort();

    QModbusClient serialClient(serialPort);
    serialClient.setRetryTimes(1);
    serialClient.setTimeout(300);
    serialClient.enableDiagnosis(true);

    QSignalSpy spy(&serialClient, &QModbusClient::readRegistersFinished);

    EXPECT_CALL(*serialPort, write).Times(3);
    EXPECT_CALL(*serialPort, readAll())
        .WillOnce(Invoke([]() { return QByteArray(); }))
        .WillOnce(Invoke(
            []() { return QByteArray("\x01\x03\x02\x00\x01\x79\x84", 7); }))
        .WillOnce(
            Invoke([]() { return QByteArray("\x01\x83\x06\xc1\x32", 5); }));
    // make sure the client is opened
    serialClient.open();
    EXPECT_EQ(serialClient.isOpened(), true);

    /// send the request
    serialClient.readRegisters(kServerAddress, kReadHoldingRegisters,
                               Address(0x05), Quantity(0x01));
    serialClient.readRegisters(kServerAddress,
                               FunctionCode::kReadHoldingRegisters,
                               Address(0x05), Quantity(0x01));

    /// wait for the operation can work done, because
    /// in rtu mode, the request must be send after t3.5
    QTest::qWait(5000);
    EXPECT_EQ(spy.count(), 2);
    RuntimeDiagnosis diagnosis = serialClient.runtimeDiagnosis();

    /// timout(1) + + retry-success(1) + failed(1)
    EXPECT_EQ(diagnosis.totalFrameNumbers(), 3);

    EXPECT_EQ(diagnosis.successedFrameNumbers(), 1);
    EXPECT_EQ(diagnosis.failedFrameNumbers(), 2);
    EXPECT_EQ(diagnosis.servers().size(), 1);

    auto servers = diagnosis.servers();
    EXPECT_NE(servers.find(kServerAddress), servers.end());
    const auto &server = servers[kServerAddress];
    EXPECT_EQ(server.errorRecords().size(), 2);
    EXPECT_EQ(server.errorRecords()[0].error(), Error::kTimeout);
    EXPECT_EQ(server.errorRecords()[1].error(), Error::kSlaveDeviceBusy);
  }

  QTimer::singleShot(1, [&]() { app.quit(); });
  app.exec();
}

template <TransferMode mode>
static void createReadCoils(ServerAddress serverAddress, Address startAddress,
                            Quantity quantity, Session &session) {
  SingleBitAccess access;

  access.setStartAddress(kStartAddress);
  access.setQuantity(kQuantity);

  for (int i = 0; i < access.quantity(); i++) {
    access.setValue(access.startAddress() + i,
                    i % 2 == 0 ? BitValue::kOn : BitValue::kOff);
  }

  session.request.reset(new Request());
  session.request->setServerAddress(kServerAddress);
  session.request->setFunctionCode(FunctionCode::kReadCoils);
  session.request->setDataChecker(MockReadCoilsDataChecker::newDataChecker());
  session.request->setData(access.marshalReadRequest());
  session.request->setUserData(access);

  session.response.setServerAddress(kServerAddress);
  session.response.setFunctionCode(FunctionCode::kReadCoils);
  session.response.setData(access.marshalReadResponse());
  session.response.setDataChecker(MockReadCoilsDataChecker::newDataChecker());

  ByteArray aduWithoutCrcRequest = session.request->marshalAduWithoutCrc();
  ByteArray aduWithoutCrcResponse = session.response.marshalAduWithoutCrc();
  if (mode == TransferMode::kRtu) {
    session.requestRaw = marshalRtuFrame(aduWithoutCrcRequest);
    session.responseRaw = marshalRtuFrame(aduWithoutCrcResponse);
  } else if (mode == TransferMode::kAscii) {
    session.requestRaw = marshalAsciiFrame(aduWithoutCrcRequest);
    session.responseRaw = marshalAsciiFrame(aduWithoutCrcResponse);
  }
}

static std::unique_ptr<Request> createSingleBitAccessRequest() {
  SingleBitAccess access;

  access.setStartAddress(kStartAddress);
  access.setQuantity(kQuantity);

  std::unique_ptr<Request> request(new Request);

  request->setServerAddress(kServerAddress);
  request->setFunctionCode(FunctionCode::kReadCoils);
  request->setDataChecker(MockReadCoilsDataChecker::newDataChecker());
  request->setData(access.marshalReadRequest());
  request->setUserData(access);
  return request;
}

static std::unique_ptr<Request> createBrocastRequest() {
  SingleBitAccess access;

  access.setStartAddress(kStartAddress);
  access.setQuantity(kQuantity);

  std::unique_ptr<Request> request(new Request);

  request->setServerAddress(Adu::kBrocastAddress);
  request->setFunctionCode(FunctionCode::kReadCoils);
  request->setDataChecker(MockReadCoilsDataChecker::newDataChecker());
  request->setData(access.marshalReadRequest());
  request->setUserData(access);
  return request;
}
