#ifndef MODBUS_TEST_MOCKER_H
#define MODBUS_TEST_MOCKER_H

#include <gtest/gtest.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QTimer>
#include <gmock/gmock.h>
#include <modbus/base/modbus.h>
#include <modbus/base/modbus_tool.h>
#include <modbus/tools/modbus_serial_client.h>

/**
 * a mock of serial port
 * we use this class for testing of modbus::QModbusClient
 */
class MockSerialPort : public modbus::AbstractSerialPort {
public:
  MockSerialPort(QObject *parent = nullptr)
      : modbus::AbstractSerialPort(parent) {
    setupCallName();
  }
  ~MockSerialPort() {}
 /* MOCK_METHOD0(void, open, (), (override));
  MOCK_METHOD(void, close, (), (override));
  MOCK_METHOD(void, write, (const char *data, size_t size), (override));
  MOCK_METHOD(QByteArray, readAll, (), (override));
  MOCK_METHOD(void, clear, (), (override));
  MOCK_METHOD(std::string, name, (), (override));*/
  MOCK_METHOD0(open, void());
  MOCK_METHOD0(close, void());
  MOCK_METHOD2(write, void(const char *data, size_t size));
  MOCK_METHOD0(readAll, QByteArray());
  MOCK_METHOD0(clear, void());
  MOCK_METHOD0(name, std::string());


  void setupCallName() {
      EXPECT_CALL(*this, name).WillRepeatedly(testing::Invoke([&]() { return "COM1"; }));
  }

  void setupDelegate() {
      ON_CALL(*this, open).WillByDefault(testing::Invoke([&]() { emit opened(); }));
      ON_CALL(*this, close).WillByDefault(testing::Invoke([&]() { emit closed(); }));
  }
  void setupOpenSuccessWriteFailedDelegate() {
      ON_CALL(*this, open).WillByDefault(testing::Invoke([&]() { emit opened(); }));
      ON_CALL(*this, close).WillByDefault(testing::Invoke([&]() { emit closed(); }));
      ON_CALL(*this, write).WillByDefault(testing::Invoke([&](const char *data, size_t size) {
      emit error("write serial failed");
    }));
  }
  void setupOpenFailed() {
    ON_CALL(*this, open).WillByDefault(testing::Invoke([&]() {
      emit error("open serial failed");
    }));
  }

  void setupTestForWrite() {
      ON_CALL(*this, open).WillByDefault(testing::Invoke([&]() { emit opened(); }));
      ON_CALL(*this, close).WillByDefault(testing::Invoke([&]() { emit closed(); }));
      ON_CALL(*this, write).WillByDefault(testing::Invoke([&](const char *data, size_t size) {
      sendoutData_.insert(sendoutData_.end(), data, data + size);
      emit bytesWritten(size);
    }));
  }

  void setupTestForWriteRead() {
      ON_CALL(*this, open).WillByDefault(testing::Invoke([&]() { emit opened(); }));
      ON_CALL(*this, close).WillByDefault(testing::Invoke([&]() { emit closed(); }));
      ON_CALL(*this, write).WillByDefault(testing::Invoke([&](const char *data, size_t size) {
      emit bytesWritten(size);
      QTimer::singleShot(10, [&]() { emit readyRead(); });
    }));
  }
  modbus::ByteArray sendoutData() { return sendoutData_; }

private:
  modbus::ByteArray sendoutData_;
};

class MockReadCoilsDataChecker {
public:
  static modbus::DataChecker newDataChecker() {
    modbus::DataChecker dataChecker;
    dataChecker.calculateRequestSize = modbus::bytesRequired<4>;
    dataChecker.calculateResponseSize = modbus::bytesRequiredStoreInArrayIndex0;
    return dataChecker;
  };
};

#endif /* MODBUS_TEST_MOCKER_H */
