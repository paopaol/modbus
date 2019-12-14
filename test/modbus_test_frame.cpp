#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <modbus/base/modbus.h>
#include <modbus/base/modbus_tool.h>
#include <modbus/base/modbus_types.h>
#include <modbus/base/single_bit_access.h>
#include <modbus_frame.h>

static modbus::Adu
createSingleBitAccessAdu(modbus::ServerAddress serverAddress,
                         modbus::FunctionCode functionCode,
                         const modbus::SingleBitAccess &access);

static modbus::DataChecker readCoilDataChecker = {
    modbus::bytesRequired<4>, modbus::bytesRequiredStoreInArrayIndex0};

TEST(ModbusFrame, marshalRtuFrame_success) {
  modbus::SingleBitAccess access;
  std::unique_ptr<modbus::Frame> rtuFrame(new modbus::RtuFrame);

  access.setStartAddress(0x10);
  access.setQuantity(0x0a);
  rtuFrame->setAdu(
      createSingleBitAccessAdu(0x01, modbus::FunctionCode::kReadCoils, access));

  modbus::ByteArray data = rtuFrame->marshal();
  modbus::ByteArray requestArray(
      {0x01, modbus::FunctionCode::kReadCoils, 0x00, 0x10, 0x00, 0x0a});
  requestArray = modbus::tool::appendCrc(requestArray);
  EXPECT_EQ(data, requestArray);
}

//      header + adu + tail
// mbap   ---  mbap + adu + tail
// rtu    --- adu + crc
// ascii  --- : + ascii(adu)+ lrc + \r\n
TEST(ModbusFrame, marshalAsciiFrame_success) {
  modbus::SingleBitAccess access;
  std::unique_ptr<modbus::Frame> asciiFrame(new modbus::AsciiFrame);

  access.setStartAddress(0x10);
  access.setQuantity(0x0a);
  asciiFrame->setAdu(
      createSingleBitAccessAdu(0x01, modbus::FunctionCode::kReadCoils, access));
  //:0x01 0x01 0x00 0x10 0x00 0x0a lrc \r\n
  modbus::ByteArray frameArray = asciiFrame->marshal();
  EXPECT_THAT(frameArray,
              testing::ElementsAre(':', '0', '1', '0', '1', '0', '0', '1', '0',
                                   '0', '0', '0', 'a', 'e', '4', '\r', '\n'));
}

static modbus::Adu
createSingleBitAccessAdu(modbus::ServerAddress serverAddress,
                         modbus::FunctionCode functionCode,
                         const modbus::SingleBitAccess &access) {
  modbus::Adu adu;
  adu.setServerAddress(0x01);
  adu.setFunctionCode(modbus::kReadCoils);
  adu.setData(access.marshalReadRequest());
  adu.setDataChecker(readCoilDataChecker);
  return adu;
}
