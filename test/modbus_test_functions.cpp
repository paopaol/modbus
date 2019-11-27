#include <gtest/gtest.h>
#include <modbus/base/functions.h>

TEST(modbusSingleBitAccess, marshalRequest) {
  modbus::SingleBitAccess access;

  access.setStartAddress(1);
  access.setQuantity(8);
  modbus::ByteArray expectPayload({0x00, 0x01, 0x00, 0x08});
  modbus::ByteArray payload = access.marshalRequest();

  EXPECT_EQ(expectPayload, payload);
}

TEST(modbusSingleBitAccess, unmarshalResponse_dataIsValid_unmarshalSuccess) {
  modbus::SingleBitAccess access;

  access.setStartAddress(0x13);
  access.setQuantity(0x13);

  modbus::ByteArray goodData(
      {0x03, 0xcd /*1100 1101*/, 0x6b, 0x05 /*0000 0101*/});
  bool ok = access.unmarshalResponse(goodData);
  EXPECT_EQ(ok, true);
  EXPECT_EQ(access.value(0x13), modbus::BitValue::kOn);
  EXPECT_EQ(access.value(0x14), modbus::BitValue::kOff);
  EXPECT_EQ(access.value(0x15), modbus::BitValue::kOn);
  EXPECT_EQ(access.value(0x16), modbus::BitValue::kOn);
  EXPECT_EQ(access.value(23), modbus::BitValue::kOff);
  EXPECT_EQ(access.value(24), modbus::BitValue::kOff);
  EXPECT_EQ(access.value(25), modbus::BitValue::kOn);
  EXPECT_EQ(access.value(26), modbus::BitValue::kOn);

  EXPECT_EQ(access.value(35), modbus::BitValue::kOn);
  EXPECT_EQ(access.value(36), modbus::BitValue::kOff);
  EXPECT_EQ(access.value(37), modbus::BitValue::kOn);
}
