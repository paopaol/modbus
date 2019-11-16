#include "modbus_test_mocker.h"
#include <QSignalSpy>
#include <QTimer>

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
