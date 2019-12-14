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

///      header + adu + tail
/// mbap  mbap + adu + tail
/// rtu  -- + adu + crc
/// ascii : + ascii(adu + lrc
// TEST(ModbusFrame, constructor2) {
//   modbus::Frame *frame;
//   modbus::Adu adu;
//   modbus::ByteArray data;
//   modbus::DataChecker::Result result = frame->unmarshal(data);
//   if (result == modbus::DataChecker::Result::kNeedMoreData) {
//     return;
//   }
//   frame->adu();
//   frame->serverAddress();

// }
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
