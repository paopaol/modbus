#include <gtest/gtest.h>
#include <modbus/base/functions.h>

TEST(modbusSingleBitAccess, marshalReadRequest) {
  modbus::SingleBitAccess access;

  access.setStartAddress(1);
  access.setQuantity(8);
  modbus::ByteArray expectPayload({0x00, 0x01, 0x00, 0x08});
  modbus::ByteArray payload = access.marshalReadRequest();

  EXPECT_EQ(expectPayload, payload);
}

TEST(modbusSingleBitAccess, set_get) {
  {
    modbus::SingleBitAccess access;
    access.setStartAddress(1);
    EXPECT_EQ(1, access.startAddress());
  }

  {
    modbus::SingleBitAccess access;
    access.setQuantity(8);
    EXPECT_EQ(8, access.quantity());
  }

  {
    modbus::SingleBitAccess access;
    access.setValue(modbus::BitValue::kOn);
    EXPECT_EQ(modbus::BitValue::kOn, access.value(access.startAddress()));
  }

  {
    modbus::SingleBitAccess access;
    access.setValue(0x1234 /*no exists*/, modbus::BitValue::kOn);
    EXPECT_EQ(modbus::BitValue::kBadValue, access.value(access.startAddress()));
  }
}

TEST(modbusSingleBitAccess, marshalSingleWriteRequest) {
  modbus::SingleBitAccess access;

  access.setStartAddress(0xac);
  /**
   * singleWrite must set quantity to 1
   */
  access.setQuantity(1);
  access.setValue(modbus::BitValue::kOn);

  modbus::ByteArray expectPayload({0x00, 0xac, 0xff, 0x00});

  modbus::ByteArray payload = access.marshalSingleWriteRequest();
  EXPECT_EQ(expectPayload, payload);
}

TEST(modbusSingleBitAccess, marshalMultipleWriteRequest) {
  modbus::SingleBitAccess access;
  modbus::Address startAddress = 0x13;

  access.setStartAddress(startAddress);
  access.setQuantity(10);

  // cd 01
  // cd
  // 1100 1101
  access.setValue(startAddress, modbus::BitValue::kOn);
  access.setValue(startAddress + 1, modbus::BitValue::kOff);
  access.setValue(startAddress + 2, modbus::BitValue::kOn);
  access.setValue(startAddress + 3, modbus::BitValue::kOn);

  access.setValue(startAddress + 4, modbus::BitValue::kOff);
  access.setValue(startAddress + 5, modbus::BitValue::kOff);
  access.setValue(startAddress + 6, modbus::BitValue::kOn);
  access.setValue(startAddress + 7, modbus::BitValue::kOn);
  // 01
  // 0000 0001
  access.setValue(startAddress + 8, modbus::BitValue::kOn);
  access.setValue(startAddress + 9, modbus::BitValue::kOff);

  modbus::ByteArray expectData({0x00, 0x13, 0x00, 0x0a, 0x02, 0xcd, 0x01});
  modbus::ByteArray data = access.marshalMultipleWriteRequest();
  EXPECT_EQ(data, expectData);
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
