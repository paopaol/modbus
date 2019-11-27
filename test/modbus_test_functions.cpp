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

  access.setStartAddress(1);
  access.setQuantity(3);

  // address:   8               1(in this test, start address is 1)
  // bit value: 0 0 0 0   0 1 0 1 ---> a0
  modbus::ByteArray goodData(
      {0x01 /*bytes bumber*/, 0x05 /*coil/register status*/});
  bool ok = access.unmarshalResponse(goodData);
  EXPECT_EQ(ok, true);
  EXPECT_EQ(access.value(1), modbus::BitValue::kOn);
  EXPECT_EQ(access.value(2), modbus::BitValue::kOff);
  EXPECT_EQ(access.value(3), modbus::BitValue::kOn);
}
