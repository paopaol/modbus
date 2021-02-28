#include "base/modbus_frame.h"
#include "modbus/base/modbus.h"
#include "modbus/base/modbus_types.h"
#include "modbus_test_mocker.h"
#include <atomic>

using namespace modbus;

TEST(ModbusRtuFrameDecoder, Construct) {
  ModbusRtuFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());
  decoder.Clear();
  EXPECT_FALSE(decoder.IsDone());
  EXPECT_EQ(decoder.LasError(), Error::kNoError);
}

TEST(ModbusRtuFrameDecoder, client_decode_readCoils_response_success) {
  const ByteArray expect({0x01, 0x01, 0x01, 0x05, 0x91, 0x8b});
  pp::bytes::Buffer buffer;
  buffer.Write(expect);

  Adu adu;
  ModbusRtuFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Decode(buffer, &adu);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kNoError, decoder.LasError());
  EXPECT_EQ(adu.serverAddress(), 0x01);
  EXPECT_EQ(adu.functionCode(), 0x01);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x01, 0x05));
}

TEST(ModbusRtuFrameDecoder, client_decode_writeSingleCoil_response_success) {
  pp::bytes::Buffer buffer;
  buffer.Write("\x01\x05\x00\x05\xff\x00\x9c\x3b", 8);

  Adu adu;
  ModbusRtuFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Decode(buffer, &adu);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kNoError, decoder.LasError());
  EXPECT_EQ(adu.serverAddress(), 0x01);
  EXPECT_EQ(adu.functionCode(), 0x05);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x00, 0x05, 0xff, 0x00));
}

TEST(ModbusRtuFrameDecoder, client_decode_writeMultipleCoils_response_success) {
  pp::bytes::Buffer buffer;
  buffer.Write("\x01\x0f\x00\x05\x00\x09\x85\xcc", 8);

  Adu adu;
  ModbusRtuFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Decode(buffer, &adu);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kNoError, decoder.LasError());
  EXPECT_EQ(adu.serverAddress(), 0x01);
  EXPECT_EQ(adu.functionCode(), 0x0f);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x00, 0x05, 0x00, 0x09));
}

TEST(ModbusRtuFrameDecoder, client_decode_readInputDiscrete_response_success) {
  pp::bytes::Buffer buffer;
  buffer.Write("\x01\x02\x01\x05\x61\x8b", 6);

  Adu adu;
  ModbusRtuFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Decode(buffer, &adu);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kNoError, decoder.LasError());
  EXPECT_EQ(adu.serverAddress(), 0x01);
  EXPECT_EQ(adu.functionCode(), 0x02);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x01, 0x05));
}

TEST(ModbusRtuFrameDecoder,
     client_decode_readHoldingRegisters_response_success) {
  pp::bytes::Buffer buffer;
  buffer.Write("\x01\x03\x08\x00\x01\x00\x02\x00\x03\x00\x04\x0d\x14", 13);

  Adu adu;
  ModbusRtuFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Decode(buffer, &adu);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kNoError, decoder.LasError());
  EXPECT_EQ(adu.serverAddress(), 0x01);
  EXPECT_EQ(adu.functionCode(), 0x03);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x08, 0x00, 0x01, 0x00, 0x02,
                                                 0x00, 0x03, 0x00, 0x04));
}

TEST(ModbusRtuFrameDecoder, client_decode_readSingleRegister_response_success) {
  pp::bytes::Buffer buffer;
  buffer.Write("\x01\x06\x00\x05\x00\x01\x58\x0b", 8);

  Adu adu;
  ModbusRtuFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Decode(buffer, &adu);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kNoError, decoder.LasError());
  EXPECT_EQ(adu.serverAddress(), 0x01);
  EXPECT_EQ(adu.functionCode(), 0x06);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x00, 0x05, 0x00, 0x01));
}

TEST(ModbusRtuFrameDecoder,
     client_decode_writeMultipleRegisters_response_success) {
  pp::bytes::Buffer buffer;
  buffer.Write("\x01\x10\x00\x05\x00\x03\x90\x09", 8);

  Adu adu;
  ModbusRtuFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Decode(buffer, &adu);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kNoError, decoder.LasError());
  EXPECT_EQ(adu.serverAddress(), 0x01);
  EXPECT_EQ(adu.functionCode(), 0x10);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x00, 0x05, 0x00, 0x03));
}

TEST(ModbusRtuFrameDecoder,
     client_decode_readWriteMultipleRegisters_response_success) {
  pp::bytes::Buffer buffer;
  buffer.Write("\x01\x17\x06\x00\x01\x00\x02\x00\x03\xfd\x8b", 11);

  Adu adu;
  ModbusRtuFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Decode(buffer, &adu);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kNoError, decoder.LasError());
  EXPECT_EQ(adu.serverAddress(), 0x01);
  EXPECT_EQ(adu.functionCode(), 0x17);
  EXPECT_THAT(adu.data(),
              ::testing::ElementsAre(0x06, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03));
}

TEST(ModbusRtuFrameDecoder, client_decode_response_error_response) {
  const ByteArray expect({0x01, 0x8f, 0x06, 0xc4, 0x32});
  pp::bytes::Buffer buffer;
  buffer.Write(expect);

  Adu adu;
  ModbusRtuFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Decode(buffer, &adu);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kSlaveDeviceBusy, decoder.LasError());
  EXPECT_EQ(adu.serverAddress(), 0x01);
  EXPECT_EQ(adu.functionCode(), 0x0f);
  EXPECT_THAT(adu.data(), ::testing::ElementsAre(0x06));
}

TEST(ModbusRtuFrameDecoder, client_decode_response_crc_error_response) {
  const ByteArray expect({0x01, 0x8f, 0x06, 0xc4, 0x33});
  pp::bytes::Buffer buffer;
  buffer.Write(expect);

  Adu adu;
  ModbusRtuFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Decode(buffer, &adu);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kStorageParityError, decoder.LasError());
}

TEST(ModbusRtuFrameDecoder,
     client_decode_readWriteMultipleRegisters_response_needmordata) {
  // complete frame:\x01\x17\x06\x00\x01\x00\x02\x00\x03\xfd\x8b
  pp::bytes::Buffer buffer;
  buffer.Write("\x01\x17\x06\x00", 4);

  Adu adu;
  ModbusRtuFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Clear();

  EXPECT_EQ(CheckSizeResult::kNeedMoreData, decoder.Decode(buffer, &adu));
  EXPECT_EQ(false, decoder.IsDone());
}

TEST(ModbusRtuFrameDecoder, decode_bad_funtioncode) {
  pp::bytes::Buffer buffer;
  buffer.Write("\x01\x55\x06\x00\x01\x00\x02\x00\x03\xfd\x8b", 11);

  Adu adu;
  ModbusRtuFrameDecoder decoder(creatDefaultCheckSizeFuncTableForClient());

  decoder.Clear();

  decoder.Decode(buffer, &adu);
  EXPECT_EQ(true, decoder.IsDone());
  EXPECT_EQ(Error::kIllegalFunctionCode, decoder.LasError());
}
