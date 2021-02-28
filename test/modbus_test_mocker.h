#ifndef MODBUS_TEST_MOCKER_H
#define MODBUS_TEST_MOCKER_H

#include <gtest/gtest.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QTimer>
#include <gmock/gmock.h>
#include <modbus/base/modbus.h>
#include <modbus/base/modbus_tool.h>
#include <modbus/tools/modbus_client.h>

using namespace testing;

/**
 * a mock of serial port
 * we use this class for testing of modbus::QModbusClient
 */
class MockSerialPort : public modbus::AbstractIoDevice {
  Q_OBJECT
public:
  MockSerialPort(QObject *parent = nullptr) : modbus::AbstractIoDevice(parent) {
    setupCallName();
  }
  ~MockSerialPort() {}
  MOCK_METHOD0(open, void());
  MOCK_METHOD0(close, void());
  MOCK_METHOD2(write, void(const char *data, size_t size));
  MOCK_METHOD0(readAll, QByteArray());
  MOCK_METHOD0(clear, void());
  MOCK_METHOD0(name, std::string());

  void setupCallName() {
    ON_CALL(*this, open).WillByDefault(Invoke([&]() { emit opened(); }));
    ON_CALL(*this, close).WillByDefault(Invoke([&]() { emit closed(); }));
    ON_CALL(*this, write(_, _))
        .WillByDefault(Invoke([&](const char *data, size_t size) {
          emit bytesWritten(size);
          emit readyRead();
        }));
    ON_CALL(*this, clear()).WillByDefault([&]() {});

    EXPECT_CALL(*this, clear()).WillRepeatedly([&]() {});
    EXPECT_CALL(*this, name).WillRepeatedly(Return("COM1"));
    EXPECT_CALL(*this, open).WillRepeatedly(Invoke([&]() { emit opened(); }));
    EXPECT_CALL(*this, close).WillRepeatedly(Invoke([&]() { emit closed(); }));
  }

  /* void setupOpenSuccessWriteFailedDelegate() { */
  /*   ON_CALL(*this, open).WillByDefault(Invoke([&]() { emit opened(); })); */
  /*   ON_CALL(*this, close).WillByDefault(Invoke([&]() { emit closed(); })); */
  /*   ON_CALL(*this, write) */
  /*       .WillByDefault(Invoke([&](const char *data, size_t size) { */
  /*         emit error("write serial failed"); */
  /*       })); */
  /* } */
  /* void setupOpenFailed() { */
  /*   ON_CALL(*this, open).WillByDefault(Invoke([&]() { */
  /*     emit error("open serial failed"); */
  /*   })); */
  /* } */

  /* void setupTestForWrite() { */
  /*   ON_CALL(*this, open).WillByDefault(Invoke([&]() { emit opened(); })); */
  /*   ON_CALL(*this, close).WillByDefault(Invoke([&]() { emit closed(); })); */
  /*   ON_CALL(*this, write) */
  /*       .WillByDefault(Invoke( */
  /*           [&](const char *data, size_t size) { emit bytesWritten(size);
   * })); */
  /* } */

  /* void setupTestForWriteRead() { */
  /*   ON_CALL(*this, open).WillByDefault(Invoke([&]() { emit opened(); })); */
  /*   ON_CALL(*this, close).WillByDefault(Invoke([&]() { emit closed(); })); */
  /*   ON_CALL(*this, write) */
  /*       .WillByDefault(Invoke([&](const char *data, size_t size) { */
  /*         emit bytesWritten(size); */
  /*         QTimer::singleShot(10, [&]() { emit readyRead(); }); */
  /*       })); */
  /* } */
};


#endif /* MODBUS_TEST_MOCKER_H */
