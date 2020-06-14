#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <modbus/base/modbus.h>
#include <modbus/base/modbus_tool.h>
#include <modbus/base/modbus_types.h>
#include <modbus_frame.h>

using namespace modbus;

TEST(modbusFrame, rtuMarshalFrame) {
  const ByteArray data({0x00, 0x01, 0x00, 0x01, 0x00, 0x11});
  const ByteArray expect({0x00, 0x01, 0x00, 0x01, 0x00, 0x11, 0xAC, 0x17});
  auto actual = marshalRtuFrame(data);
  EXPECT_EQ(actual, expect);
}

TEST(modbusFrame, asciiMarshalFrame) {
  const ByteArray data({0x00, 0x01, 0x00, 0x01, 0x00, 0x11});
  const ByteArray expect({':', 0x30, 0x30, 0x30, 0x31, 0x30, 0x30, 0x30, 0x31,
                          0x30, 0x30, 0x31, 0x31, 0x45, 0x44, '\r', '\n'});

  auto actual = marshalAsciiFrame(data);
  EXPECT_EQ(actual, expect);
}

TEST(ModbusFrame, marshalRtuFrame_success) {
  ByteArray expect({0x01, 0x01, 0x00, 0x10, 0x00, 0x0a});

  Adu adu;
  adu.setServerAddress(0x01);
  adu.setFunctionCode(FunctionCode::kReadCoils);
  adu.setDataChecker({bytesRequiredStoreInArrayIndex<0>});
  adu.setData({0x00, 0x10, 0x00, 0x0a});

  RtuFrame rtuFrame;
  rtuFrame.setAdu(adu);

  ByteArray data = rtuFrame.marshal();
  expect = tool::appendCrc(expect);
  EXPECT_EQ(data, expect);
}

//      header + adu + tail
// mbap   ---  mbap + adu + tail
// rtu    --- adu + crc
// ascii  --- : + ascii(adu)+ lrc + \r\n
TEST(ModbusFrame, marshalAsciiFrame_success) {
  ByteArray expect({0x01, 0x01, 0x00, 0x10, 0x00, 0x0a});

  Adu adu;
  adu.setServerAddress(0x01);
  adu.setFunctionCode(FunctionCode::kReadCoils);
  adu.setDataChecker({bytesRequiredStoreInArrayIndex<0>});
  adu.setData({0x00, 0x10, 0x00, 0x0a});

  AsciiFrame asciiFrame;

  asciiFrame.setAdu(adu);
  ByteArray frameArray = asciiFrame.marshal();
  EXPECT_THAT(frameArray,
              testing::ElementsAre(':', '0', '1', '0', '1', '0', '0', '1', '0',
                                   '0', '0', '0', 'A', 'E', '4', '\r', '\n'));
}

TEST(ModbusFrame, unmarshalAsciiFrame_success) {
  // rtu   - 0x01 0x01 0x02 0x33 0x33
  // ascii - ":010102333396\r\n"
  ByteArray data({':', '0', '1', '0', '1', '0', '2', '3', '3', '3', '3', '9',
                  '6', '\r', '\n'});
  Adu adu;
  adu.setDataChecker({bytesRequiredStoreInArrayIndex<0>});

  AsciiFrame asciiFrame;
  asciiFrame.setAdu(adu);

  struct Result {
    ByteArray data;
    DataChecker::Result ret;
  };
  std::vector<Result> tests = {
      {{':', '0'}, DataChecker::Result::kNeedMoreData},
      {{':', '0', '1', '0', '1'}, DataChecker::Result::kNeedMoreData},
      {{':', '0', '1', '0', '1', '0', '2', '3', '3'},
       DataChecker::Result::kNeedMoreData},
      {{':', '0', '1', '0', '1', '0', '2', '3', '3', '3', '3'},
       DataChecker::Result::kNeedMoreData},
      {{':', '0', '1', '0', '1', '0', '2', '3', '3', '3', '3', '9', '6', '\r',
        '\n'},
       DataChecker::Result::kSizeOk},
  };
  for (const auto &test : tests) {
    Error error = Error::kNoError;
    auto ret = asciiFrame.unmarshal(test.data, &error);
    EXPECT_EQ(ret, test.ret);
  }
}

TEST(ModbusFrame, MbapFrame_marshalSize_success) {
  Adu adu;
  adu.setServerAddress(0x01);
  adu.setFunctionCode(FunctionCode::kReadCoils);
  adu.setDataChecker({bytesRequiredStoreInArrayIndex<0>});
  adu.setData({0x00, 0x10, 0x00, 0x0a});

  MbapFrame frame;
  frame.setAdu(adu);

  EXPECT_EQ(frame.marshalSize(), 12);
}

TEST(ModbusFrame, MbapFrame_marshal_success) {
  ByteArray expect(
      {0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x01, 0x01, 0x00, 0x00, 0x00, 0x02});

  Adu adu;
  adu.setServerAddress(0x01);
  adu.setFunctionCode(FunctionCode::kReadCoils);
  adu.setDataChecker({bytesRequiredStoreInArrayIndex<0>});
  adu.setData({0x00, 0x00, 0x00, 0x02});

  MbapFrame frame;
  frame.setAdu(adu);

  EXPECT_EQ(frame.marshal(), expect);
}

TEST(Modbusframe, MbapFrame_unmrshal_success) {
  struct Test {
    ByteArray data;
    DataChecker::Result ret;
    Error errRet;
  };

  std::vector<Test> tests = {
      {{0x00, 0x00, 0x00, 0x00, 0x00},
       DataChecker::Result::kNeedMoreData,
       Error::kNoError},
      {{0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x01, 0x01},
       DataChecker::Result::kNeedMoreData,
       Error::kNoError},
      {{0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x01, 0x01, 0x00, 0x00, 0x00, 0x02},
       DataChecker::Result::kSizeOk,
       Error::kNoError},
  };

  Adu adu;
  adu.setDataChecker({bytesRequiredStoreInArrayIndex<0>});

  MbapFrame frame;
  frame.setAdu(adu);

  for (const auto &test : tests) {
    Error error = Error::kNoError;
    auto result = frame.unmarshal(test.data, &error);
    EXPECT_EQ(result, test.ret);
    EXPECT_EQ(error, test.errRet);
  }
}
