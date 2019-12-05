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

TEST(SixteenBitAccess, setgetValue) {
  modbus::SixteenBitAccess access;

  access.setStartAddress(0x00);
  access.setValue(0x05);
  EXPECT_EQ(0x05, access.value(0x00).toUint16());

  access.setValue(0x01, 1);
  EXPECT_EQ(1, access.value(0x01).toUint16());

  access.setValue(0x02, 4);
  EXPECT_EQ(4, access.value(0x02).toUint16());

  // test not exists values
  bool ok;
  access.value(0x1000, &ok);
  EXPECT_EQ(false, ok);
}

TEST(SixteenBitAccess, setGetDeviceName) {
  modbus::SixteenBitAccess access;

  access.setDeviceName("device name");
  EXPECT_EQ("device name", access.deviceName());
}

TEST(SixteenBitAccess, setGetValueEx) {
  modbus::SixteenBitAccess access;

  access.setStartAddress(0x01);
  access.setQuantity(1);
  access.setDescription(access.startAddress(), "value-name-1");
  access.setValue(0x03);
  auto valueEx = access.valueEx(access.startAddress());
  EXPECT_EQ(valueEx.value.toUint16(), 0x03);
  EXPECT_EQ(valueEx.description, "value-name-1");
}

TEST(SixteenBitAccess, setGetValueEx2) {
  modbus::SixteenBitAccess access;

  access.setStartAddress(0x01);
  access.setQuantity(2);

  access.setDescription(access.startAddress(), "value-name-1");
  access.setValue(access.startAddress(), 0x0101);

  access.setDescription(access.startAddress() + 1, "value-name-2");
  access.setValue(access.startAddress() + 1, 0x0202);

  auto valueEx = access.valueEx(access.startAddress());
  EXPECT_EQ(valueEx.value.toUint16(), 0x0101);
  EXPECT_EQ(valueEx.description, "value-name-1");

  valueEx = access.valueEx(access.startAddress() + 1);
  EXPECT_EQ(valueEx.value.toUint16(), 0x0202);
  EXPECT_EQ(valueEx.description, "value-name-2");
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

  access.setDescription(access.startAddress(), "value1");

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
  EXPECT_EQ(false, success);

  modbus::ByteArray badResponse2({0x05, 0x02, 0x2b, 0x00, 0x00, 0x00});
  success = access.unmarshalReadResponse(badResponse2);
  EXPECT_EQ(false, success);
}
