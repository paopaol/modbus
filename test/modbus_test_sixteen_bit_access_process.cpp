#include <gtest/gtest.h>
#include <modbus/base/sixteen_bit_access.h>
#include <modbus/tools/modbus_client.h>

TEST(SixteenBitAccessProcess, ProcessReadMultipleRegisters) {
  const modbus::DataChecker dataChecker = {
      modbus::bytesRequiredStoreInArrayIndex<0>};

  modbus::SixteenBitAccess access;
  access.setStartAddress(0);
  access.setQuantity(3);

  modbus::Request request = modbus::createRequest(
      0x01, modbus::FunctionCode::kReadHoldingRegisters, dataChecker, access,
      access.marshalMultipleReadRequest());
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
