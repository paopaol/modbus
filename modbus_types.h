#ifndef __MODBUS_TYPES_H_
#define __MODBUS_TYPES_H_
#include <cstdint>

namespace modbus {
using ServerAddress = uint8_t;
using CoilAddress = uint16_t;
using RegisterAddress = uint16_t;
using Quantity = uint16_t;
enum class CoilStatus { kOn, kOff };
enum FunctionCode { kReadCoil };
} // namespace modbus

#endif // __MODBUS_TYPES_H_
