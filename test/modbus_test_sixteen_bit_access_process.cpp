#include <gtest/gtest.h>
#include <modbus/base/sixteen_bit_access.h>

TEST(SixteenBitAccessProcess, ProcessReadMultipleRegisters) {
  modbus::SixteenBitAccess access;
  access.setStartAddress(0);
  access.setQuantity(3);
  access.setDeviceName("device-1");
  access.setDescription(modbus::Address(0x00), "humidity");
  access.setDescription(modbus::Address(0x01), "temperature");
  access.setDescription(modbus::Address(0x02), "CO2 concentration");

  modbus::Request request = modbus::createReadRegistersRequest(
      0x01, modbus::FunctionCode::kReadHoldingRegisters, access);
  modbus::Response response;

  response.setError(modbus::Error::kNoError);
  response.setFunctionCode(modbus::FunctionCode::kReadHoldingRegisters);
  response.setServerAddress(0x01);
  response.setDataChecker(request.dataChecker());
  response.setData(
      modbus::ByteArray({0x06, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03}));

  bool ok = modbus::processReadRegisters(request, response, &access);
  EXPECT_EQ(ok, true);
  EXPECT_EQ(access.value(0x00).toUint16(), 1);
  EXPECT_EQ(access.value(0x01).toUint16(), 2);
  EXPECT_EQ(access.value(0x02).toUint16(), 3);
}
