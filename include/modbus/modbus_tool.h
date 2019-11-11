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
};

} // namespace modbus

#endif // __MODBUS_TOOL_H_
