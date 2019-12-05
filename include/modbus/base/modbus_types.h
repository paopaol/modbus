#ifndef __MODBUS_TYPES_H_
#define __MODBUS_TYPES_H_
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace modbus {
using ByteArray = std::vector<uint8_t>;
using ServerAddress = uint8_t;
using CoilAddress = uint16_t;
using RegisterAddress = uint16_t;
using Address = uint16_t;
using Quantity = uint16_t;
enum class BitValue { kOff, kOn, kBadValue };

inline std::ostream &operator<<(std::ostream &output, const BitValue &value) {
  switch (value) {
  case BitValue::kOn:
    output << "on";
    break;
  case BitValue::kOff:
    output << "off";
    break;
  case BitValue::kBadValue:
    output << "badValue";
    break;
  default:
    output.setstate(std::ios_base::failbit);
  }
  return output;
}

struct BitValueEx {
  BitValue value = BitValue::kBadValue;
  std::string description;
};

struct SixteenBitValue {
  enum class ByteOrder { kNetworkByteOrder, kHostByteOrder };

  SixteenBitValue(uint8_t chFirst, uint8_t chSecond)
      : chFirst_(chFirst), chSecond_(chSecond) {}
  SixteenBitValue() {}
  SixteenBitValue(uint16_t value) { setUint16(value); }
  ~SixteenBitValue() {}

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
    char hex[16] = {0};
    snprintf(hex, sizeof(hex), "%02x %02x",
             static_cast<unsigned char>(chFirst_),
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

struct SixteenBitValueEx {
  SixteenBitValue value;
  std::string description;
};

enum FunctionCode { kInvalidCode = 0x00, kReadCoils = 0x01 };
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

enum class LogLevel { kDebug, kWarning, kInfo };
using LogWriter = std::function<void(LogLevel level, const std::string &msg)>;

} // namespace modbus

#endif // __MODBUS_TYPES_H_
