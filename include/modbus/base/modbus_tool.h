#ifndef __MODBUS_TOOL_H_
#define __MODBUS_TOOL_H_

#include "modbus.h"

namespace modbus {
class tool {
public:
  static inline std::string dumpHex(const ByteArray &byteArray) {
    std::string hexString;
    char hex[3] = {0};
    for (const auto &ch : byteArray) {
      sprintf(hex, "%02x", static_cast<unsigned char>(ch));
      hexString += hex;
    }
    return hexString;
  }

  static uint16_t crc16_modbus(const uint8_t *data, size_t size);
  /**
   * Calculate the crc check of data and then return data+crc.
The crc check is added in the order of the first low order and the high order.
   */
  static inline ByteArray appendCrc(const ByteArray &data) {
    uint16_t crc = crc16_modbus((uint8_t *)(data.data()), data.size());
    auto dataWithCrc = data;
    /// first push low bit
    dataWithCrc.push_back(crc % 256);
    /// second push high bit
    dataWithCrc.push_back(crc / 256);
    return dataWithCrc;
  }
};

} // namespace modbus

#endif // __MODBUS_TOOL_H_
