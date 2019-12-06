#ifndef __MODBUS_TOOL_H_
#define __MODBUS_TOOL_H_

#include "modbus_types.h"

namespace modbus {
class tool {
public:
  static inline std::string dumpHex(const ByteArray &byteArray,
                                    const std::string &delimiter = " ") {
    std::string hexString;
    char hex[4096] = {0};
    for (const auto &ch : byteArray) {
      snprintf(hex, sizeof(hex), "%02x%s", static_cast<unsigned char>(ch),
               delimiter.c_str());
      hexString += hex;
    }
    return hexString;
  }

  static uint16_t crc16_modbus(const uint8_t *data, size_t size);
  static uint8_t lrc_modbus(const uint8_t *data, size_t len);
  /**
   * Calculate the crc check of data and then return data+crc.
The crc check is added in the order of the first low order and the high
order.
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

  static inline ByteArray appendLrc(const ByteArray &data) {
    uint8_t lrc = lrc_modbus((uint8_t *)data.data(), data.size());
    auto dataWithLrc = data;
    dataWithLrc.push_back(lrc);

    return dataWithLrc;
  }

  static inline ByteArray subArray(const ByteArray &array, size_t index,
                                   int n = -1) {
    if (n == -1) {
      return ByteArray(array.begin() + index, array.end());
    } else {
      return ByteArray(array.begin() + index, array.begin() + index + n);
    }
  }
}; // namespace modbus

} // namespace modbus

#endif // __MODBUS_TOOL_H_
