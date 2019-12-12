#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <modbus/base/modbus_types.h>

TEST(SixteenBitValue, construct) {
  modbus::SixteenBitValue v1(0x11, 0x22);
  EXPECT_THAT(v1.twoBytes(), testing::ElementsAre(0x11, 0x22));
  EXPECT_EQ(v1.toUint16(), 0x1122);
  EXPECT_EQ(v1.toUint16(modbus::SixteenBitValue::ByteOrder::kHostByteOrder),
            0x1122);
  EXPECT_EQ(v1.toUint16(modbus::SixteenBitValue::ByteOrder::kNetworkByteOrder),
            0x2211);

  modbus::SixteenBitValue v2(0x1122);
  EXPECT_THAT(v1.twoBytes(), testing::ElementsAre(0x11, 0x22));

  modbus::SixteenBitValue v3;
}

TEST(SixteenBitValue, equal_test) {
  modbus::SixteenBitValue v1(0x11, 0x22);
  auto v2 = v1;
  EXPECT_EQ(v1, v2);
}
