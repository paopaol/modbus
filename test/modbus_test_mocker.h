#include <gtest/gtest.h>

#include <QByteArray>
#include <QCoreApplication>
#include <gmock/gmock.h>
#include <modbus/base/modbus.h>
#include <modbus/base/modbus_tool.h>
#include <modbus/tools/modbus_serial_client.h>

/**
 * a mock of serial port
 * we use this class for testing of modbus::QSerialClient
 */
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

  void setupTestForWrite() {
    ON_CALL(*this, open).WillByDefault([&]() { emit opened(); });
    ON_CALL(*this, close).WillByDefault([&]() { emit closed(); });
    ON_CALL(*this, write).WillByDefault([&](const char *data, size_t size) {
      sendoutData_.insert(sendoutData_.end(), data, data + size);
      emit bytesWritten(size);
    });
  }
  modbus::ByteArray sendoutData() { return sendoutData_; }

private:
  modbus::ByteArray sendoutData_;
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
