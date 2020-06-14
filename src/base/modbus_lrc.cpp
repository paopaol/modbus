#include <modbus/base/modbus_tool.h>

namespace modbus {
uint8_t tool::lrc_modbus(const uint8_t *data, size_t len) {
  unsigned char lrc = 0;

  while (len--) {
    lrc += *data++;
  }

  return (~lrc) + 1;
}

} // namespace modbus
