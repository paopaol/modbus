#include <gtest/gtest.h>
#include <modbus/base/modbus.h>
#include <modbus/base/modbus_tool.h>
#include <modbus/base/single_bit_access.h>
#include <modbus_frame.h>

TEST(TestData, dump_dumpByteArray_outputIsHexString) {
  uint8_t binary[5] = {0x01, 0x33, 0x4b, 0xab, 0x3b};
  modbus::ByteArray byteArray(binary, binary + 5);

  auto hexString = modbus::tool::dumpHex(byteArray);
  EXPECT_EQ(hexString, "01 33 4b ab 3b ");
}

TEST(modbusFrame, rtuMarshalFrame) {

  modbus::SingleBitAccess access;

  access.setStartAddress(0x01);
  access.setQuantity(0x11);

  modbus::Request request;

  request.setServerAddress(0x00);
  request.setFunctionCode(modbus::FunctionCode::kReadCoils);
  request.setData(access.marshalReadRequest());

  auto rtuFrame = modbus::marshalRtuFrame(request.marshalAduWithoutCrc());
  EXPECT_EQ(rtuFrame, modbus::ByteArray({0x00, modbus::FunctionCode::kReadCoils,
                                         0x00, 0x01, 0x00, 0x11, 0xac, 0x17}));
}

TEST(modbusFrame, asciiMarshalFrame) {

  modbus::SingleBitAccess access;

  access.setStartAddress(0x01);
  access.setQuantity(0x11);

  modbus::Request request;

  request.setServerAddress(0x00);
  request.setFunctionCode(modbus::FunctionCode::kReadCoils);
  request.setData(access.marshalReadRequest());

  auto rtuFrame = modbus::marshalAsciiFrame(request.marshalAduWithoutCrc());
  EXPECT_EQ(rtuFrame, modbus::ByteArray({':', 0x30, 0x30, 0x30, 0x31, 0x30,
                                         0x30, 0x30, 0x31, 0x30, 0x30, 0x31,
                                         0x31, 0x65, 0x64, '\r', '\n'}));
}
