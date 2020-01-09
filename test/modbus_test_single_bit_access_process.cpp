#include <gtest/gtest.h>
#include <modbus/base/single_bit_access.h>
#include <modbus/tools/modbus_client.h>

TEST(SingleBitAccessProcess, readSingleBitProcess) {
  modbus::DataChecker dataChecker = {modbus::bytesRequiredStoreInArrayIndex<0>};

  modbus::SingleBitAccess access;

  access.setStartAddress(0x03);
  access.setQuantity(0x03);
  access.setDeviceName("device");
  access.setDescription(access.startAddress(), "value1");
  access.setDescription(access.startAddress() + 1, "value2");
  access.setDescription(access.startAddress() + 2, "value3");

  auto request =
      modbus::createRequest(0x01, modbus::FunctionCode::kReadCoils, dataChecker,
                            access, access.marshalReadRequest());
  modbus::Response response;

  response.setServerAddress(0x01);
  response.setFunctionCode(modbus::FunctionCode::kReadCoils);
  response.setError(modbus::Error::kNoError);
  response.setDataChecker(request.dataChecker());
  response.setData(modbus::ByteArray({0x01, 0x05}));

  bool ok = modbus::processReadSingleBit(request, response, &access);
  EXPECT_EQ(ok, true);
  EXPECT_EQ(access.value(access.startAddress()), modbus::BitValue::kOn);
  EXPECT_EQ(access.value(access.startAddress() + 1), modbus::BitValue::kOff);
  EXPECT_EQ(access.value(access.startAddress() + 2), modbus::BitValue::kOn);
}
