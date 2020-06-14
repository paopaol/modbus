#include <modbus/base/modbus_tool.h>

namespace modbus {

static void invert_uint8(uint8_t *dest, uint8_t *src);
static void invert_uint16(uint16_t *dest, uint16_t *src);

uint16_t tool::crc16_modbus(const uint8_t *data, size_t size) {
  uint16_t in = 0xFFFF;
  uint16_t poly = 0x8005;
  uint8_t ch = 0;

  while (size--) {
    ch = *(data++);
    invert_uint8(&ch, &ch);
    in ^= (ch << 8);
    for (int i = 0; i < 8; i++) {
      if (in & 0x8000)
        in = (in << 1) ^ poly;
      else
        in = in << 1;
    }
  }
  invert_uint16(&in, &in);
  return (in);
}

void invert_uint8(uint8_t *dest, uint8_t *src) {
  int i;
  uint8_t tmp[4];
  tmp[0] = 0;
  for (i = 0; i < 8; i++) {
    if (src[0] & (1 << i))
      tmp[0] |= 1 << (7 - i);
  }
  dest[0] = tmp[0];
}

void invert_uint16(uint16_t *dest, uint16_t *src) {
  int i;
  uint16_t tmp[4];
  tmp[0] = 0;
  for (i = 0; i < 16; i++) {
    if (src[0] & (1 << i))
      tmp[0] |= 1 << (15 - i);
  }
  dest[0] = tmp[0];
}
} // namespace modbus
