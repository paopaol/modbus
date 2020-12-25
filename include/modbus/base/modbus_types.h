#ifndef __MODBUS_TYPES_H_
#define __MODBUS_TYPES_H_
#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace modbus {
using ByteArray = std::vector<uint8_t>;
using ServerAddress = uint8_t;
using CoilAddress = uint16_t;
using RegisterAddress = uint16_t;
using Address = uint16_t;
using Quantity = uint16_t;

struct SixteenBitValue {
  enum class ByteOrder { kNetworkByteOrder, kHostByteOrder };

  SixteenBitValue(uint8_t chFirst, uint8_t chSecond)
      : chFirst_(chFirst), chSecond_(chSecond) {}
  SixteenBitValue() {}
  explicit SixteenBitValue(uint16_t value) { setUint16(value); }
  ~SixteenBitValue() {}

  void setFirstByte(uint8_t byte) { chFirst_ = byte; }
  void setSecondByte(uint8_t byte) { chSecond_ = byte; }

  uint8_t firstByte() const { return chFirst_; }
  uint8_t secondByte() const { return chSecond_; }
  ByteArray twoBytes() const { return ByteArray({chFirst_, chSecond_}); }

  uint16_t toUint16(ByteOrder order = ByteOrder::kHostByteOrder) const {
    uint16_t value;
    if (order == ByteOrder::kHostByteOrder) {
      value = chFirst_ * 256 + chSecond_;
    } else {
      value = chFirst_ + chSecond_ * 256;
    }
    return value;
  }

  void setUint16(uint16_t value) {
    chFirst_ = value / 256;
    chSecond_ = value % 256;
  }

  std::string toHexString() {
    char hex[32] = {0};
    sprintf(hex, "%02x %02x", static_cast<unsigned char>(chFirst_),
            static_cast<unsigned char>(chSecond_));
    return hex;
  }

  SixteenBitValue &operator=(uint16_t value) {
    setUint16(value);

    return *this;
  }

  bool operator==(const SixteenBitValue &value) const {
    return value.chFirst_ == chFirst_ && value.chSecond_ == chSecond_;
  }

private:
  uint8_t chFirst_ = 0;
  uint8_t chSecond_ = 0;
};

enum FunctionCode {
  kInvalidCode = 0x00,
  kReadCoils = 0x01,
  kReadInputDiscrete = 0x02,
  kReadHoldingRegisters = 0x03,
  kReadInputRegister = 0x04,
  kWriteSingleCoil = 0x05,
  kWriteSingleRegister = 0x06,
  kWriteMultipleRegisters = 0x10,
  kWriteMultipleCoils = 0x0f,
  kReadFileRecords = 0x14,
  kWriteFileRecords = 0x15,
  kMaskWriteRegister = 0x16,
  kReadWriteMultipleRegisters = 0x17,
  kReadDeviceIdentificationCode = 0x2b
};
inline std::ostream &operator<<(std::ostream &output,
                                const FunctionCode &code) {
  switch (code) {
  case FunctionCode::kInvalidCode:
    output << "invalid function code";
    break;
  case FunctionCode::kReadCoils:
    output << "read coils";
    break;
  case FunctionCode::kReadInputDiscrete:
    output << "read input discrete";
    break;
  case FunctionCode::kReadHoldingRegisters:
    output << "read holding registers";
    break;
  case FunctionCode::kReadInputRegister:
    output << "read input registers";
    break;
  case FunctionCode::kWriteSingleCoil:
    output << "write single coil";
    break;
  case FunctionCode::kWriteSingleRegister:
    output << "write single register";
    break;
  case FunctionCode::kWriteMultipleRegisters:
    output << "write multiple registers";
    break;
  case FunctionCode::kWriteMultipleCoils:
    output << "write multiple coils";
    break;
  case FunctionCode::kReadFileRecords:
    output << "read file records";
    break;
  case FunctionCode::kWriteFileRecords:
    output << "write file records";
    break;
  case FunctionCode::kMaskWriteRegister:
    output << "mask write register";
    break;
  case FunctionCode::kReadWriteMultipleRegisters:
    output << "read/write multiple registers";
    break;
  case FunctionCode::kReadDeviceIdentificationCode:
    output << "read device identification code";
    break;
  default:
    output << "function code(" << std::to_string(static_cast<int>(code)) << ")";
    break;
  }

  return output;
}

enum class Error {
  kNoError = 0,
  kIllegalFunctionCode = 0x01,
  kIllegalDataAddress = 0x02,
  kIllegalDataValue = 0x03,
  kSlaveDeviceFailure = 0x04,
  kConfirm = 0x05,
  kSlaveDeviceBusy = 0x06,
  kStorageParityError = 0x08,
  kUnavailableGatewayPath = 0x0a,
  kGatewayTargetDeviceResponseLoss = 0x0b,

  /// user defined error, not inlcuded in modbus protocol
  kTimeout = 0x1000
};
inline std::ostream &operator<<(std::ostream &output, const Error &error) {
  switch (error) {
  case Error::kNoError:
    output << "NoError";
    break;
  case Error::kIllegalFunctionCode:
    output << "Illegal function";
    break;
  case Error::kIllegalDataAddress:
    output << "Illegal data address";
    break;
  case Error::kIllegalDataValue:
    output << "Illegal data value";
    break;
  case Error::kSlaveDeviceFailure:
    output << "Slave device failure";
    break;
  case Error::kConfirm:
    output << "confirm";
    break;
  case Error::kSlaveDeviceBusy:
    output << "Slave device is busy";
    break;
  case Error::kStorageParityError:
    output << "Storage parity error";
    break;
  case Error::kUnavailableGatewayPath:
    output << "Unavailable gateway path";
    break;
  case Error::kGatewayTargetDeviceResponseLoss:
    output << "Gateway target device failed to respond";
    break;
  case Error::kTimeout:
    output << "Timeout";
    break;
  default:
    output.setstate(std::ios_base::failbit);
  }

  return output;
}
enum class TransferMode { kRtu, kAscii, kMbap };

enum class LogLevel { kDebug, kWarning, kInfo, kError };
using LogWriter = std::function<void(LogLevel level, const std::string &msg)>;

class RuntimeDiagnosis {
public:
  RuntimeDiagnosis() {}
  ~RuntimeDiagnosis() {}

  class ErrorRecord {
  public:
    ErrorRecord(FunctionCode functionCode, Error error,
                const ByteArray &requestFrame)
        : functionCode_(functionCode), error_(error),
          requestFrame_(requestFrame), occurCount_(1) {}

    FunctionCode functionCode() const { return functionCode_; }
    Error error() const { return error_; }
    size_t occurrenceCount() const { return occurCount_; }
    void incrementOccurCount() { occurCount_++; }
    ByteArray requestFrame() const { return requestFrame_; }

    bool operator==(const ErrorRecord &other) {
      return other.functionCode_ == functionCode_ && other.error_ == error_ &&
             other.requestFrame_ == requestFrame_;
    }

  private:
    FunctionCode functionCode_ = FunctionCode::kInvalidCode;
    Error error_ = Error::kNoError;
    ByteArray requestFrame_;
    size_t occurCount_ = 0;
  };

  using ErrorRecordList = std::vector<ErrorRecord>;
  class Server {
  public:
    Server(ServerAddress serverAddress) : serverAddress_(serverAddress) {}
    Server() {}
    ServerAddress serverAddress() const { return serverAddress_; }
    ErrorRecordList errorRecords() const { return errorRecordList_; }

    void insertErrorRecord(FunctionCode functionCode, Error error,
                           const ByteArray &requestFrame) {
      ErrorRecord errorRecord(functionCode, error, requestFrame);
      auto it = std::find(errorRecordList_.begin(), errorRecordList_.end(),
                          errorRecord);
      if (it != errorRecordList_.end()) {
        it->incrementOccurCount();
        return;
      }
      errorRecordList_.push_back(errorRecord);
    }

  private:
    ServerAddress serverAddress_;
    ErrorRecordList errorRecordList_;
  };

  using ServerMap = std::map<ServerAddress, Server>;

  ServerMap servers() const { return servers_; }

  void insertErrorRecord(ServerAddress serverAddress, FunctionCode functionCode,
                         Error error, const ByteArray &requestFrame) {
    incrementtotalFrameNumbers();
    auto it = servers_.find(serverAddress);
    if (it == servers_.end()) {
      Server server(serverAddress);
      servers_[serverAddress] = server;
      it = servers_.find(serverAddress);
    }
    auto &server = it->second;
    server.insertErrorRecord(functionCode, error, requestFrame);
  }

  size_t totalFrameNumbers() const { return totalFrameNumbers_; }

  void incrementtotalFrameNumbers() { totalFrameNumbers_++; }

  size_t failedFrameNumbers() const {
    size_t number = 0;
    for (const auto &el : servers_) {
      const auto &server = el.second;
      const auto &errorRecords = server.errorRecords();
      for (const auto &errorRecord : errorRecords) {
        number += errorRecord.occurrenceCount();
      }
    }
    return number;
  }

  size_t successedFrameNumbers() const {
    return totalFrameNumbers() - failedFrameNumbers();
  }

private:
  size_t totalFrameNumbers_ = 0;
  ServerMap servers_;
};

} // namespace modbus

namespace std {
template <typename T> std::string to_string(const T &t) {
  std::stringstream s;

  s << t;
  return s.str();
}
} // namespace std

#endif // __MODBUS_TYPES_H_
