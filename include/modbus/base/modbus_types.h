#ifndef __MODBUS_TYPES_H_
#define __MODBUS_TYPES_H_
#include <cstdint>
#include <vector>

namespace modbus {
using ServerAddress = uint8_t;
using CoilAddress = uint16_t;
using RegisterAddress = uint16_t;
using Quantity = uint16_t;
enum class BitValue { kOn, kOff };
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

using ByteArray = std::vector<uint8_t>;

} // namespace modbus

#endif // __MODBUS_TYPES_H_
