#include <gtest/gtest.h>
#include <modbus/base/modbus.h>
#include <modbus/base/modbus_tool.h>
#include <modbus/base/single_bit_access.h>
#include <modbus_frame.h>

TEST(TestData, dump_dumpByteArray_outputIsHexString) {
  uint8_t binary[5] = {0x01, 0x33, 0x4b, 0xab, 0x3b};
  modbus::ByteArray byteArray(binary, binary + 5);

  auto hexString = modbus::tool::dumpHex(byteArray);
  EXPECT_EQ(hexString, " 01 33 4b ab 3b");
}
