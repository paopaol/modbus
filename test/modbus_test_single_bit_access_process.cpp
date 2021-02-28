#include <gtest/gtest.h>
#include <modbus/base/single_bit_access.h>
#include <modbus/tools/modbus_client.h>

TEST(SingleBitAccessProcess, readSingleBitProcess) {
  modbus::SingleBitAccess access;

  access.setStartAddress(0x03);
  access.setQuantity(0x03);

  auto request = modbus::createRequest(0x01, modbus::FunctionCode::kReadCoils,
                                       access, access.marshalReadRequest());
  modbus::Response response;

  response.setServerAddress(0x01);
  response.setFunctionCode(modbus::FunctionCode::kReadCoils);
  response.setError(modbus::Error::kNoError);
  response.setData(modbus::ByteArray({0x01, 0x05}));

  bool ok = modbus::processReadSingleBit(request, response, &access);
  EXPECT_EQ(ok, true);
  EXPECT_EQ(access.value(access.startAddress()), true);
  EXPECT_EQ(access.value(access.startAddress() + 1), false);
  EXPECT_EQ(access.value(access.startAddress() + 2), true);
}
