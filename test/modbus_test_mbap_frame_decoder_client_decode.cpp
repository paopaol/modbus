#include "base/modbus_frame.h"
#include "modbus/base/modbus.h"
#include "modbus/base/modbus_types.h"
#include "modbus_test_mocker.h"
#include <atomic>

using namespace modbus;

TEST(ModbusMbapFrameDecoder, Construct) {
  ModbusMbapFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());
  decoder.Clear();
  EXPECT_FALSE(decoder.IsDone());
  EXPECT_EQ(decoder.LasError(), Error::kNoError);
}

TEST(ModbusMbapFrameDecoder, client_decode_readCoils_response_success) {
  const ByteArray expect(
      {0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x01, 0x01, 0x01, 0x05});
  pp::bytes::Buffer buffer;
  buffer.Write(expect);

  Adu adu;
  ModbusMbapFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Decode(buffer, &adu);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kNoError, decoder.LasError());
  EXPECT_EQ(adu.transactionId(), 0x01);
  EXPECT_EQ(adu.serverAddress(), 0x01);
  EXPECT_EQ(adu.functionCode(), 0x01);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x01, 0x05));
}

TEST(ModbusMbapFrameDecoder, client_decode_writeSingleCoil_response_success) {
  pp::bytes::Buffer buffer;
  buffer.Write("\x00\x01\x00\x00\x00\x06\x01\x05\x00\x05\xff\x00", 12);

  Adu adu;
  ModbusMbapFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Decode(buffer, &adu);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kNoError, decoder.LasError());
  EXPECT_EQ(adu.transactionId(), 0x01);
  EXPECT_EQ(adu.serverAddress(), 0x01);
  EXPECT_EQ(adu.functionCode(), 0x05);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x00, 0x05, 0xff, 0x00));
}

TEST(ModbusMbapFrameDecoder,
     client_decode_writeMultipleCoils_response_success) {
  pp::bytes::Buffer buffer;
  buffer.Write("\x01\x02\x00\x00\x00\x06\x01\x0f\x00\x05\x00\x09", 12);

  Adu adu;
  ModbusMbapFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Decode(buffer, &adu);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kNoError, decoder.LasError());
  EXPECT_EQ(adu.transactionId(), 258);
  EXPECT_EQ(adu.serverAddress(), 0x01);
  EXPECT_EQ(adu.functionCode(), 0x0f);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x00, 0x05, 0x00, 0x09));
}

TEST(ModbusMbapFrameDecoder, client_decode_readInputDiscrete_response_success) {
  pp::bytes::Buffer buffer;
  buffer.Write("\x01\x02\x00\x00\x00\x04\x01\x02\x01\x05", 10);

  Adu adu;
  ModbusMbapFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Decode(buffer, &adu);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kNoError, decoder.LasError());
  EXPECT_EQ(adu.transactionId(), 258);
  EXPECT_EQ(adu.serverAddress(), 0x01);
  EXPECT_EQ(adu.functionCode(), 0x02);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x01, 0x05));
}

TEST(ModbusMbapFrameDecoder,
     client_decode_readHoldingRegisters_response_success) {
  pp::bytes::Buffer buffer;
  buffer.Write(
      "\x01\x02\x00\x00\x00\x0b\x01\x03\x08\x00\x01\x00\x02\x00\x03\x00\x04",
      17);

  Adu adu;
  ModbusMbapFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Decode(buffer, &adu);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kNoError, decoder.LasError());
  EXPECT_EQ(adu.transactionId(), 258);
  EXPECT_EQ(adu.serverAddress(), 0x01);
  EXPECT_EQ(adu.functionCode(), 0x03);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x08, 0x00, 0x01, 0x00, 0x02,
                                                 0x00, 0x03, 0x00, 0x04));
}

TEST(ModbusMbapFrameDecoder,
     client_decode_readSingleRegister_response_success) {
  pp::bytes::Buffer buffer;
  buffer.Write("\x01\x02\x00\x00\x00\x06\x01\x06\x00\x05\x00\x01", 12);

  Adu adu;
  ModbusMbapFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Decode(buffer, &adu);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kNoError, decoder.LasError());
  EXPECT_EQ(adu.transactionId(), 258);
  EXPECT_EQ(adu.serverAddress(), 0x01);
  EXPECT_EQ(adu.functionCode(), 0x06);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x00, 0x05, 0x00, 0x01));
}

TEST(ModbusMbapFrameDecoder,
     client_decode_writeMultipleRegisters_response_success) {
  pp::bytes::Buffer buffer;
  buffer.Write("\x01\x02\x00\x00\x00\x06\x01\x10\x00\x05\x00\x03", 12);

  Adu adu;
  ModbusMbapFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Decode(buffer, &adu);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kNoError, decoder.LasError());
  EXPECT_EQ(adu.transactionId(), 258);
  EXPECT_EQ(adu.serverAddress(), 0x01);
  EXPECT_EQ(adu.functionCode(), 0x10);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x00, 0x05, 0x00, 0x03));
}

TEST(ModbusMbapFrameDecoder,
     client_decode_readWriteMultipleRegisters_response_success) {
  pp::bytes::Buffer buffer;
  buffer.Write("\x01\x02\x00\x00\x00\x09\x01\x17\x06\x00\x01\x00\x02\x00\x03",
               15);

  Adu adu;
  ModbusMbapFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Decode(buffer, &adu);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kNoError, decoder.LasError());
  EXPECT_EQ(adu.transactionId(), 258);
  EXPECT_EQ(adu.serverAddress(), 0x01);
  EXPECT_EQ(adu.functionCode(), 0x17);
  EXPECT_THAT(adu.data(),
              ::testing::ElementsAre(0x06, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03));
}

TEST(ModbusMbapFrameDecoder, client_decode_response_error_response) {
  const ByteArray expect({0x01, 002, 0x00, 0x00, 0x00, 0x03, 0x01, 0x8f, 0x06});
  pp::bytes::Buffer buffer;
  buffer.Write(expect);

  Adu adu;
  ModbusMbapFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Decode(buffer, &adu);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kSlaveDeviceBusy, decoder.LasError());
  EXPECT_EQ(adu.transactionId(), 258);
  EXPECT_EQ(adu.serverAddress(), 0x01);
  EXPECT_EQ(adu.functionCode(), 0x0f);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x06));
}

TEST(ModbusMbapFrameDecoder,
     client_decode_readWriteMultipleRegisters_response_needmordata) {
  // complete frame:\x01\x17\x06\x00\x01\x00\x02\x00\x03\xfd\x8b
  pp::bytes::Buffer buffer;
  buffer.Write("\x01\x02\x00\x00\x00\x09\x01\x17\x06\x00", 10);

  Adu adu;
  ModbusMbapFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Clear();

  EXPECT_EQ(CheckSizeResult::kNeedMoreData, decoder.Decode(buffer, &adu));
  EXPECT_FALSE(decoder.IsDone());
}

TEST(ModbusMbapFrameDecoder, decode_bad_funtioncode) {
  pp::bytes::Buffer buffer;
  buffer.Write("\x01\x02\x00\x00\x00\x09\x01\x55\x06\x00\x01\x00\x02\x00\x03",
               15);

  Adu adu;
  ModbusMbapFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Clear();

  decoder.Decode(buffer, &adu);
  EXPECT_TRUE(decoder.IsDone());
  EXPECT_EQ(Error::kIllegalFunctionCode, decoder.LasError());
}
