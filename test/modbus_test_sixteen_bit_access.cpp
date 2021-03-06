#include <gtest/gtest.h>
#include <modbus/base/sixteen_bit_access.h>

TEST(SixteenBitAccess, setgetStartAddress) {
  modbus::SixteenBitAccess access;

  access.setStartAddress(3);
  EXPECT_EQ(3, access.startAddress());
}

TEST(SixteenBitAccess, setgetQuantity) {
  modbus::SixteenBitAccess access;

  access.setQuantity(3);
  EXPECT_EQ(3, access.quantity());
}

TEST(SixteenBitAccess, setValue_outOfRange) {
  modbus::SixteenBitAccess access;

  access.setStartAddress(0x00);
  access.setQuantity(0x01);

  access.setValue(0x02, 4);
  bool ok;
  access.value(0x1000, &ok);
  EXPECT_FALSE(ok);
}

TEST(SixteenBitAccess, setgetValue) {
  modbus::SixteenBitAccess access;

  access.setStartAddress(0x00);
  access.setQuantity(0x10);
  access.setValue(0x05);
  EXPECT_EQ(0x05, access.value(0x00).toUint16());

  access.setValue(0x01, 1);
  EXPECT_EQ(1, access.value(0x01).toUint16());

  access.setValue(0x02, 4);
  EXPECT_EQ(4, access.value(0x02).toUint16());

  // test not exists values
  bool ok;
  access.value(0x1000, &ok);
  EXPECT_FALSE(ok);
}

TEST(SixteenBitAccess, marshalMultipleReadRequest_success) {
  modbus::SixteenBitAccess access;

  access.setStartAddress(0x6b);
  access.setQuantity(0x03);

  modbus::ByteArray expectedArray({0x00, 0x6b, 0x00, 0x03});
  modbus::ByteArray array = access.marshalMultipleReadRequest();
  EXPECT_EQ(array, expectedArray);
}

TEST(SixteenBitAccess, marshalSingleWriteRequest_success) {
  modbus::SixteenBitAccess access;

  access.setStartAddress(0x01);
  access.setValue(0x03);

  modbus::ByteArray expectedArray({0x00, 0x01, 0x00, 0x03});
  auto array = access.marshalSingleWriteRequest();
  EXPECT_EQ(array, expectedArray);
}

TEST(SixteenBitAccess, marshalMultipleWriteRequest_success) {
  modbus::SixteenBitAccess access;

  access.setStartAddress(0x01);
  access.setQuantity(0x02);
  access.setValue(access.startAddress(), 0x0a);
  access.setValue(access.startAddress() + 1, 0x0102);

  modbus::ByteArray expectedArray(
      {0x00, 0x01, 0x00, 0x02, 0x04, 0x00, 0x0a, 0x01, 0x02});
  auto array = access.marshalMultipleWriteRequest();
  EXPECT_EQ(array, expectedArray);
}

TEST(SixteenBitAccess, unmarshalReadResponse_success) {
  modbus::SixteenBitAccess access;

  access.setStartAddress(0x6b);
  access.setQuantity(0x03);

  modbus::ByteArray response({0x06, 0x02, 0x2b, 0x00, 0x00, 0x00, 0x64});

  bool success = access.unmarshalReadResponse(response);
  EXPECT_EQ(true, success);
  EXPECT_EQ(0x022b, access.value(access.startAddress()).toUint16());
  EXPECT_EQ(0x00, access.value(access.startAddress() + 1).toUint16());
  EXPECT_EQ(0x64, access.value(access.startAddress() + 2).toUint16());
}

TEST(SixteenBitAccess, unmarshalReadResponse_failed) {
  modbus::SixteenBitAccess access;

  access.setStartAddress(0x6b);
  access.setQuantity(0x03);

  modbus::ByteArray badResponse({0x06, 0x02, 0x2b, 0x00, 0x00, 0x00});
  bool success = access.unmarshalReadResponse(badResponse);
  EXPECT_FALSE(success);

  modbus::ByteArray badResponse2({0x05, 0x02, 0x2b, 0x00, 0x00, 0x00});
  success = access.unmarshalReadResponse(badResponse2);
  EXPECT_FALSE(success);
}
