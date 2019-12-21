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
                                         0x00, 0x01, 0x00, 0x11, 0xAC, 0x17}));
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
                                         0x31, 0x45, 0x44, '\r', '\n'}));
}

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
                                   '0', '0', '0', 'A', 'E', '4', '\r', '\n'));
}

TEST(ModbusFrame, unmarshalAsciiFrame_success) {

  // rtu   - 0x01 0x01 0x02 0x33 0x33
  // ascii - ":010102333396\r\n"
  modbus::ByteArray data({':', '0', '1', '0', '1', '0', '2', '3', '3', '3', '3',
                          '9', '6', '\r', '\n'});
  modbus::Adu responseAdu;
  responseAdu.setDataChecker(readCoilDataChecker);

  modbus::AsciiFrame asciiFrame;
  asciiFrame.setAdu(responseAdu);
  modbus::Error error = modbus::Error::kNoError;
  EXPECT_EQ(asciiFrame.unmarshal(modbus::tool::subArray(data, 0, 2), &error),
            modbus::DataChecker::Result::kNeedMoreData);
  EXPECT_EQ(asciiFrame.unmarshal(modbus::tool::subArray(data, 0, 5), &error),
            modbus::DataChecker::Result::kNeedMoreData);
  EXPECT_EQ(asciiFrame.unmarshal(modbus::tool::subArray(data, 0, 9), &error),
            modbus::DataChecker::Result::kNeedMoreData);
  EXPECT_EQ(asciiFrame.unmarshal(modbus::tool::subArray(data, 0, 11), &error),
            modbus::DataChecker::Result::kNeedMoreData);
  EXPECT_EQ(asciiFrame.unmarshal(modbus::tool::subArray(data, 0, 15), &error),
            modbus::DataChecker::Result::kSizeOk);
}

TEST(ModbusFrame, MbapFrame_marshalSize_success) {
  modbus::SingleBitAccess access;
  access.setStartAddress(0x00);
  access.setQuantity(2);

  modbus::Adu adu =
      createSingleBitAccessAdu(0x01, modbus::FunctionCode::kReadCoils, access);

  modbus::MbapFrame frame;
  frame.setAdu(adu);

  EXPECT_EQ(frame.marshalSize(), 12);
}

TEST(ModbusFrame, MbapFrame_marshal_success) {
  modbus::SingleBitAccess access;
  access.setStartAddress(0x00);
  access.setQuantity(2);

  modbus::Adu adu =
      createSingleBitAccessAdu(0x01, modbus::FunctionCode::kReadCoils, access);

  modbus::MbapFrame frame;
  frame.setAdu(adu);

  EXPECT_THAT(frame.marshal(),
              testing::ElementsAre(0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x01,
                                   modbus::FunctionCode::kReadCoils, 0x00, 0x00,
                                   0x00, 0x02));
}

TEST(Modbusframe, MbapFrame_unmrshal_success) {
  modbus::Adu adu;
  adu.setDataChecker(readCoilDataChecker);

  modbus::Error error = modbus::Error::kNoError;
  modbus::MbapFrame frame;
  frame.setAdu(adu);

  auto result = frame.unmarshal(
      modbus::ByteArray({{0x00, 0x00, 0x00, 0x00, 0x00}}), &error);
  EXPECT_EQ(result, modbus::DataChecker::Result::kNeedMoreData);

  result = frame.unmarshal(
      modbus::ByteArray({{0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x01,
                          modbus::FunctionCode::kReadCoils}}),
      &error);
  EXPECT_EQ(result, modbus::DataChecker::Result::kNeedMoreData);

  result = frame.unmarshal(
      modbus::ByteArray(
          {{0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x01,
            modbus::FunctionCode::kReadCoils, 0x00, 0x00, 0x00, 0x02}}),
      &error);

  EXPECT_EQ(result, modbus::DataChecker::Result::kSizeOk);
  EXPECT_EQ(error, modbus::Error::kNoError);
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
